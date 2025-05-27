#pragma once

#include "Scene.h"
#include "DXRRenderer.h" // �ǉ�

enum class RenderingMode {
	TRADITIONAL,    // �ʏ탌���_�����O
	DXR_RAYTRACING  // DXR���C�g���[�V���O
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

	// DXR�֘A���\�b�h��ǉ�
	template<typename T = Scene, typename... Args >
	void OpenDXRScene(Args&&... args) {
		m_nextScene.reset(new T(std::forward<Args>(args)...));
		m_nextScene->Init();

		// DXR�V�[���̏ꍇ�͓��ʂȏ�����
		auto* dxrScene = dynamic_cast<DXRScene*>( m_nextScene.get() );
		if ( dxrScene && m_dxrRenderer ) {
			dxrScene->InitDXR(m_dxrRenderer.get());
			dxrScene->BuildAccelerationStructures();
		}

		if ( !m_scene )
			m_scene = std::move(m_nextScene);
	}

	// �����_�����O���[�h�؂�ւ�
	void SetRenderingMode(RenderingMode mode);
	RenderingMode GetRenderingMode() const { return m_renderingMode; }

	// DXR�����_���[�擾
	DXRRenderer* GetDXRRenderer() const { return m_dxrRenderer.get(); }


	unsigned long GetFrame() { return m_frame; }


private:
	friend class Singleton<GameManager>;

	GameManager() {};
	~GameManager() {};

	bool InitDXR();

	Renderer* m_renderer = nullptr;
	std::unique_ptr<DXRRenderer> m_dxrRenderer = nullptr; // �ǉ�


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