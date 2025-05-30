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

	m_useDXR = true; // DXR���g�p����ꍇ��true�ɐݒ�


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

		// DXR�V�[�����ύX���ꂽ�ꍇ�A�A�N�Z�����[�V�����\�����č\�z
		if ( m_useDXR && m_dxrRenderer ) {
			// �K�v�ɉ�����DXRRenderer�ɍč\�z��ʒm
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
				// �ʏ�̃��X�^���C�[�[�V����
				m_renderer->InitFrame();

				{
					std::unique_lock<std::mutex> lock(m_updateMutex);
					// TODO: Scene��PreDraw����
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
				// DXR�����_�����O
				m_renderer->InitFrameForDXR();  // �V�������\�b�h
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
		// TODO: �X���b�h�G���[���Ƀ��C���X���b�h��~����
		// std::atomic<bool>�Ńt���O���Ă�
		MessageBoxA(nullptr, e.what(), "�G���[", MB_OK);
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
	// IMGUI�E�B���h�E�J�n
	ImGui::Begin("Rendering Options");

	// DXR�؂�ւ��`�F�b�N�{�b�N�X
	bool currentUseDXR = m_useDXR;
	if ( ImGui::Checkbox("Use DXR Raytracing", &currentUseDXR) ) {
		m_useDXR = currentUseDXR;
	}

	// ���݂̕`������\��
	ImGui::Text("Current Rendering: %s", m_useDXR ? "DXR Raytracing" : "Rasterization");

	// �p�t�H�[�}���X���
	ImGui::Text("Frame: %lu", m_frame);

	// DXR���L���ȏꍇ�̒ǉ��I�v�V����
	if ( m_useDXR ) {
		ImGui::Separator();
		ImGui::Text("DXR Settings");

		// �����I��DXR�ŗL�̐ݒ��ǉ��\
		// ��: �ő唽�ˉ񐔁A�T���v�����Ȃ�
	}

	ImGui::End();

	// �C�ӂ�ImGui�֐��̌Ăяo��
	// ���L�ł�"Hello, world!"�Ƃ����^�C�g���̃E�B���h�E��\������
	ImGui::Begin("Hello, world!");
	ImGui::Text("This is some useful text.");
	ImGui::End();
}
