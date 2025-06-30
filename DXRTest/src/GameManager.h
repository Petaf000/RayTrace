#pragma once

#include "Scene.h"

class Renderer;
class DXRRenderer;

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

	unsigned long GetFrame() { return m_frame; }

	// DXR関連
	void SetUseDXR(bool useDXR) { m_useDXR = useDXR; }
	bool GetUseDXR() const { return m_useDXR; }
private:
	friend class Singleton<GameManager>;

	GameManager() {};
	~GameManager() {};

	void DrawIMGUI();

	Renderer* m_renderer = nullptr;
	DXRRenderer* m_dxrRenderer = nullptr; // DXRRenderer追加


	std::unique_ptr<Scene> m_scene{};
	std::unique_ptr<Scene> m_nextScene{};

	std::mutex m_fileMutex;
	std::mutex m_updateMutex;
	std::mutex m_drawMutex;

	unsigned long m_frame{};

	DWORD m_lastTime{};
	Time m_drawTime{};


	bool m_isRunningDraw{};
	bool m_useDXR = false; // DXR使用フラグ

	tf::Executor m_executor;
	tf::Taskflow m_renderTask;
};