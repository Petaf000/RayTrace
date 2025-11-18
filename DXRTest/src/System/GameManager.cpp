#include "GameManager.h"

#include <timeapi.h>

#include "System/Renderer/Renderer.h"

#include "System/Renderer/DXRRenderer.h"
#include "Scene/CornelBoxScene.h"

#include "Input.h"

constexpr float kFpsUpdateInterval = 0.5f;

void GameManager::Init() {
	m_lastTime = timeGetTime();


	m_renderer = &Singleton<Renderer>::getInstance();
	m_renderer->Init();

	OpenScene<CornelBoxScene>();

	m_dxrRenderer = &Singleton<DXRRenderer>::getInstance();
	m_dxrRenderer->Init(m_renderer);

	m_useDXR = true; // 現在DXRのみ使用可

	StartDrawThread();
	

	Input::Init();
	// 
	//OpenScene<Game>();
}


void GameManager::UnInit() {
	WaitDraw();


	if ( m_scene )
		m_scene->UnInit();

	Input::Uninit();
	
	Singleton<DXRRenderer>::getInstance().UnInit();
	Singleton<Renderer>::getInstance().Cleanup();
	
}

void GameManager::Update() {
	if (m_hasThreadError)
		return;

	m_frame++;

	if ( m_nextScene ) {
		m_scene = std::move(m_nextScene);

		// DXRシーンが変更された場合、アクセラレーション構造を再構築
		if ( m_useDXR && m_dxrRenderer ) {
			// 必要に応じてDXRRendererに再構築を通知
			// m_dxrRenderer->RebuildAccelerationStructures();
		}
	}
		
	
	Input::Update();
	
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
				m_renderer->InitFrame();
				m_dxrRenderer->Render();
				m_dxrRenderer->RenderDXRIMGUI();
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
		m_hasThreadError = true;

		MessageBoxA(nullptr, e.what(), "レンダリングエラー", MB_OK);

		//TODO: 本来はメインスレッドでUnInitを呼び出す
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
	/*
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
	}

	ImGui::End();

	*/

	// フレームループ内で
	ImGui::Begin("Performance Info");

	// 毎フレームのデルタタイムを加算
	m_drawFpsTimeSinceUpdate += m_drawTime.DeltaTime;

	// 1秒以上経過したらFPS表示を更新
	if (m_drawFpsTimeSinceUpdate >= kFpsUpdateInterval)
	{
		float currentFps = 1.0f / m_drawTime.DeltaTime;
		m_drawFpsDisplay = currentFps; // 最新のFPSで表示用の値を更新
		m_drawFpsTimeSinceUpdate = 0.0f; // カウンターをリセット
	}

	ImGui::Text("DrawAvgFPS: %f", m_drawFpsDisplay);
	ImGui::End();

	// 最後にLastTimeを更新
	m_drawTime.LastTime = m_drawTime.Qpc.QuadPart;
}
