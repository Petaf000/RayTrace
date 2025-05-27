#pragma once

#include "Scene.h"
#include "DXRRenderer.h" // 追加

enum class RenderingMode {
	TRADITIONAL,    // 通常レンダリング
	DXR_RAYTRACING  // DXRレイトレーシング
};

class Renderer;

class GameManager {
public:
	void Init();
	void UnInit();
	void Draw();
	void Update();

	void StartDrawThread();
	void WaitDraw();

	std::unique_ptr<Scene>& GetScene() { return m_scene; };

	template<typename T = Scene, typename... Args >
	void OpenScene(Args&&... args) {
		m_nextScene.reset(new T(std::forward<Args>(args)...));

		m_nextScene->Init();

		if ( !m_scene )
			m_scene = std::move(m_nextScene);
	}

	// DXR関連メソッドを追加
	template<typename T = Scene, typename... Args >
	void OpenDXRScene(Args&&... args) {
		m_nextScene.reset(new T(std::forward<Args>(args)...));
		m_nextScene->Init();

		// DXRシーンの場合は特別な初期化
		auto* dxrScene = dynamic_cast<DXRScene*>( m_nextScene.get() );
		if ( dxrScene && m_dxrRenderer ) {
			dxrScene->InitDXR(m_dxrRenderer.get());
			dxrScene->BuildAccelerationStructures();
		}

		if ( !m_scene )
			m_scene = std::move(m_nextScene);
	}

	// レンダリングモード切り替え
	void SetRenderingMode(RenderingMode mode);
	RenderingMode GetRenderingMode() const { return m_renderingMode; }

	// DXRレンダラー取得
	DXRRenderer* GetDXRRenderer() const { return m_dxrRenderer.get(); }


	unsigned long GetFrame() { return m_frame; }


private:
	friend class Singleton<GameManager>;

	GameManager() {};
	~GameManager() {};

	bool InitDXR();

	Renderer* m_renderer = nullptr;
	std::unique_ptr<DXRRenderer> m_dxrRenderer = nullptr; // 追加


	std::unique_ptr<Scene> m_scene{};
	std::unique_ptr<Scene> m_nextScene{};

	std::mutex m_fileMutex;
	std::mutex m_updateMutex;
	std::mutex m_drawMutex;

	unsigned long m_frame{};

	DWORD m_lastTime{};

	bool m_isRunningDraw{};

	tf::Executor m_executor;
	tf::Taskflow m_renderTask;

	RenderingMode m_renderingMode = RenderingMode::TRADITIONAL;
};