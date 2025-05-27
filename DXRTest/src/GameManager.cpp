#include "GameManager.h"

#include "timeapi.h"

#include "renderer.h"

void GameManager::Init() {
	m_lastTime = timeGetTime();


	m_renderer = &Singleton<Renderer>::getInstance();
	m_renderer->Init();
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
	
	Singleton<Renderer>::getInstance().Cleanup();
}

void GameManager::Update() {
	m_frame++;

	if ( m_nextScene )
		m_scene = std::move(m_nextScene);
	
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