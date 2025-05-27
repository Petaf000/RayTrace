#include "GameManager.h"

#include "timeapi.h"

#include "renderer.h"
#include "DXRRenderer.h"
#include "DXRScene.h"

void GameManager::Init() {
	m_lastTime = timeGetTime();


	m_renderer = &Singleton<Renderer>::getInstance();
	m_renderer->Init();
	StartDrawThread();


	// DXR初期化を試行
	if ( InitDXR() ) {
		std::cout << "DXR initialization successful" << std::endl;
		m_renderingMode = RenderingMode::DXR_RAYTRACING; // デフォルトでDXR使用
	}
	else {
		std::cout << "DXR initialization failed, using traditional rendering" << std::endl;
		m_renderingMode = RenderingMode::TRADITIONAL;
	}

	//Input::Init();
	// 
	//OpenScene<Game>();
}


void GameManager::UnInit() {
	WaitDraw();


	if ( m_scene )
		m_scene->UnInit();

	//Input::Uninit();

	if ( m_dxrRenderer ) {
		m_dxrRenderer->UnInit();
		m_dxrRenderer.reset();
	}
	
	Singleton<Renderer>::getInstance().Cleanup();
}

void GameManager::Update() {
	m_frame++;

	if ( m_nextScene ) {
		m_scene = std::move(m_nextScene);

		// DXRシーンの場合は特別な初期化
		auto* dxrScene = dynamic_cast<DXRScene*>( m_scene.get() );
		if ( dxrScene && m_dxrRenderer ) {
			try {
				dxrScene->InitDXR(m_dxrRenderer.get());
				dxrScene->BuildAccelerationStructures();
				std::cout << "DXR scene initialized successfully" << std::endl;
			}
			catch ( const std::exception& e ) {
				std::cerr << "Failed to initialize DXR scene: " << e.what() << std::endl;
				// フォールバックモードに切り替え
				SetRenderingMode(RenderingMode::TRADITIONAL);
			}
		}
	}
	
	//Input::Update();
	
	if ( m_scene )
		m_scene->Update();
	
	//Singleton<Renderer>::getInstance().Update();
}


void GameManager::Draw() {
	m_isRunningDraw = true;

	while ( m_isRunningDraw ) {
		{
			std::unique_lock<std::mutex> lock(m_drawMutex);
			switch ( m_renderingMode ) {
				case RenderingMode::TRADITIONAL:
					{
						std::unique_lock<std::mutex> lock(m_updateMutex);
						// TODO: SceneのPreDraw処理
					}

					{
						Singleton<Renderer>::getInstance().Render();
						if ( m_scene )
							m_scene->Draw();
					}
					break;

				case RenderingMode::DXR_RAYTRACING:
					// DXRレイトレーシング
					if ( m_dxrRenderer && m_scene ) {
						auto* dxrScene = dynamic_cast<DXRScene*>( m_scene.get() );
						if ( dxrScene ) {
							dxrScene->Draw(); // DXRSceneのDrawがDXRRendererを呼び出す
						}
						else {
							// DXRシーンでない場合はフォールバック
							SetRenderingMode(RenderingMode::TRADITIONAL);
							{
								std::unique_lock<std::mutex> lock(m_updateMutex);
								// TODO: SceneのPreDraw処理
							}

							{
								Singleton<Renderer>::getInstance().Render();
								if ( m_scene )
									m_scene->Draw();
							}
						}
					}
					break;
			}
		}
	}
}


void GameManager::StartDrawThread() {
	
	m_renderTask.emplace([this]() { this->Draw(); }).name("DrawThread");
	try {
		m_executor.run(m_renderTask);
	}
	catch ( const std::exception& e ) {
		// TODO: スレッドエラー時にメインスレッド停止処理
		// std::atomic<bool>でフラグ立てる
		MessageBoxA(nullptr, e.what(), "エラー", MB_OK);
		UnInit();
	}
}

void GameManager::WaitDraw() {
	m_isRunningDraw = false;

	{
		std::unique_lock<std::mutex> lock(m_drawMutex);

		m_renderer->WaitGPU();
	}
}

void GameManager::SetRenderingMode(RenderingMode mode) {
	RenderingMode oldMode = m_renderingMode;
	m_renderingMode = mode;

	std::cout << "Switching rendering mode from " << (int)oldMode << " to " << (int)mode << std::endl;

	// モード切り替え時の特別な処理
	switch ( mode ) {
		case RenderingMode::TRADITIONAL:
			// 従来レンダリングモードに切り替え
			if ( oldMode == RenderingMode::DXR_RAYTRACING ) {
				std::cout << "Switched to Traditional Rendering" << std::endl;
			}
			break;

		case RenderingMode::DXR_RAYTRACING:
			// DXRレイトレーシングモードに切り替え
			if ( !m_dxrRenderer ) {
				std::cerr << "DXR renderer not available, falling back to traditional" << std::endl;
				m_renderingMode = RenderingMode::TRADITIONAL;
				return;
			}

			if ( m_scene ) {
				auto* dxrScene = dynamic_cast<DXRScene*>( m_scene.get() );
				if ( dxrScene ) {
					try {
						// アクセラレーション構造を再構築
						dxrScene->BuildAccelerationStructures();
						std::cout << "Switched to DXR Raytracing" << std::endl;
					}
					catch ( const std::exception& e ) {
						std::cerr << "Failed to rebuild acceleration structures: " << e.what() << std::endl;
						m_renderingMode = RenderingMode::TRADITIONAL;
					}
				}
				else {
					std::cerr << "Current scene is not DXR compatible, staying in traditional mode" << std::endl;
					m_renderingMode = RenderingMode::TRADITIONAL;
				}
			}
			break;
	}
}

bool GameManager::InitDXR() {
	// 前提条件チェック
	if ( !m_renderer ) {
		std::cerr << "Renderer not initialized" << std::endl;
		return false;
	}

	// DXRサポート確認
	if ( !m_renderer->CheckDXRSupport() ) {
		std::cerr << "DXR not supported on this hardware/driver" << std::endl;
		return false;
	}

	try {
		// DXRレンダラー作成
		m_dxrRenderer = std::make_unique<DXRRenderer>();

		// 既存のRendererから必要な情報を取得
		ID3D12Device5* dxrDevice = m_renderer->GetDXRDevice();
		ID3D12CommandQueue* commandQueue = m_renderer->GetCommandQueue();
		HWND hwnd = m_renderer->GetHwnd();
		UINT bufferWidth = m_renderer->GetBufferWidth();
		UINT bufferHeight = m_renderer->GetBufferHeight();

		// 取得した情報の妥当性確認
		if ( !dxrDevice || !commandQueue || !hwnd || bufferWidth == 0 || bufferHeight == 0 ) {
			std::cerr << "Invalid renderer state for DXR initialization" << std::endl;
			std::cerr << "Device5: " << ( dxrDevice ? "OK" : "NULL" ) << std::endl;
			std::cerr << "CommandQueue: " << ( commandQueue ? "OK" : "NULL" ) << std::endl;
			std::cerr << "HWND: " << ( hwnd ? "OK" : "NULL" ) << std::endl;
			std::cerr << "BufferSize: " << bufferWidth << "x" << bufferHeight << std::endl;
			m_dxrRenderer.reset();
			return false;
		}

		// DXRレンダラー初期化
		bool success = m_dxrRenderer->Init(dxrDevice, commandQueue, hwnd,
			static_cast<int>( bufferWidth ),
			static_cast<int>( bufferHeight ));
		if ( !success ) {
			std::cerr << "DXR renderer initialization failed" << std::endl;
			m_dxrRenderer.reset();
			return false;
		}

		std::cout << "DXR renderer initialized successfully" << std::endl;
		std::cout << "Resolution: " << bufferWidth << "x" << bufferHeight << std::endl;
		return true;

	}
	catch ( const std::exception& e ) {
		std::cerr << "Exception during DXR initialization: " << e.what() << std::endl;
		m_dxrRenderer.reset();
		return false;
	}
}
