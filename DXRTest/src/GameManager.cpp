#include "GameManager.h"

#include "timeapi.h"

#include "Renderer.h"

#include "DXRRenderer.h"
#include "CornelBoxScene.h"

void GameManager::Init() {
	m_lastTime = timeGetTime();


	m_renderer = &Singleton<Renderer>::getInstance();
	m_renderer->Init();

	OpenScene<CornelBoxScene>();

	m_dxrRenderer = &Singleton<DXRRenderer>::getInstance();
	m_dxrRenderer->Init(m_renderer);

	m_useDXR = true; // DXRを使用する場合はtrueに設定


	StartDrawThread();
	

	//Input::Init();
	// 
	//OpenScene<Game>();
}


void GameManager::UnInit() {
	WaitDraw();


	if ( m_scene )
		m_scene->UnInit();

	//Input::Uninit();
	
	Singleton<DXRRenderer>::getInstance().UnInit();
	Singleton<Renderer>::getInstance().Cleanup();
	
}

void GameManager::Update() {
	m_frame++;

	if ( m_nextScene ) {
		m_scene = std::move(m_nextScene);

		// DXRシーンが変更された場合、アクセラレーション構造を再構築
		if ( m_useDXR && m_dxrRenderer ) {
			// 必要に応じてDXRRendererに再構築を通知
			// m_dxrRenderer->RebuildAccelerationStructures();
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

			if ( !m_useDXR ) {
				// 通常のラスタライゼーション
				m_renderer->InitFrame();

				{
					std::unique_lock<std::mutex> lock(m_updateMutex);
					// TODO: SceneのPreDraw処理
				}

				{
					Singleton<Renderer>::getInstance().Render();
					if ( m_scene )
						m_scene->Draw();
				}

				DrawIMGUI();
				m_renderer->EndFrame();
			}
			else {
				// DXRレンダリング
				m_renderer->InitFrameForDXR();  // 新しいメソッド
				m_dxrRenderer->Render();
				DrawIMGUI();
				m_renderer->EndFrame();
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

void GameManager::DrawIMGUI() {
	// IMGUIウィンドウ開始
	ImGui::Begin("Rendering Options");

	// DXR切り替えチェックボックス
	bool currentUseDXR = m_useDXR;
	if ( ImGui::Checkbox("Use DXR Raytracing", &currentUseDXR) ) {
		m_useDXR = currentUseDXR;
	}

	// 現在の描画方式表示
	ImGui::Text("Current Rendering: %s", m_useDXR ? "DXR Raytracing" : "Rasterization");

	// パフォーマンス情報
	ImGui::Text("Frame: %lu", m_frame);

	// DXRが有効な場合の追加オプション
	if ( m_useDXR ) {
		ImGui::Separator();
		ImGui::Text("DXR Settings");

		// 将来的にDXR固有の設定を追加可能
		// 例: 最大反射回数、サンプル数など
	}

	ImGui::End();

	// 任意のImGui関数の呼び出し
	// 下記では"Hello, world!"というタイトルのウィンドウを表示する
	ImGui::Begin("Hello, world!");
	ImGui::Text("This is some useful text.");
	ImGui::End();
}
