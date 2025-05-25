#pragma once

#include "Scene.h"

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

	unsigned long GetFrame() { return m_frame; }


private:
	friend class Singleton<GameManager>;

	GameManager() {};
	~GameManager() {};


	Renderer* m_renderer = nullptr;


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
};