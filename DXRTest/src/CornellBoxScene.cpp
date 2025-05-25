#include "CornellBoxScene.h"

#include "Box.h"
#include "Plane.h"
#include "Sphere.h"
#include "RayTracingObject.h"
#include "LambertMaterial.h"
#include "MetalMaterial.h"
#include "DielectricMaterial.h"
#include "EmissiveMaterial.h"

void CornellBoxScene::Init() {
    // �ǁA���A�V��A�����̃}�e���A�����쐬
    m_Materials.push_back(std::make_unique<LambertMaterial>(XMFLOAT3(0.9f, 0.9f, 0.9f))); // ���i���A�V��A���̕ǁj
    m_Materials.push_back(std::make_unique<LambertMaterial>(XMFLOAT3(0.9f, 0.1f, 0.1f))); // �ԁi�E�ǁj
    m_Materials.push_back(std::make_unique<LambertMaterial>(XMFLOAT3(0.1f, 0.9f, 0.1f))); // �΁i���ǁj
    m_Materials.push_back(std::make_unique<EmissiveMaterial>(XMFLOAT3(1.0f, 1.0f, 1.0f), 5.0f)); // ����

    // �I�u�W�F�N�g�̃}�e���A�����쐬
    m_Materials.push_back(std::make_unique<LambertMaterial>(XMFLOAT3(0.7f, 0.7f, 0.7f))); // �O���[�i�{�b�N�X�j
    m_Materials.push_back(std::make_unique<MetalMaterial>(XMFLOAT3(0.8f, 0.8f, 0.8f), 0.1f)); // ���^���i���j
    m_Materials.push_back(std::make_unique<DielectricMaterial>(XMFLOAT3(1.0f, 1.0f, 1.0f), 1.5f)); // �K���X�i���j

    // �R�[�l���{�b�N�X�̊e�ʂ��쐬
    // ��
    auto floor = std::make_unique<Plane>(
        XMFLOAT3(0.0f, 0.0f, 0.0f),   // ���_
        XMFLOAT3(0.0f, 1.0f, 0.0f),   // ������@��
        10.0f,                         // ��
        10.0f                          // ���s��
    );
    floor->SetMaterial(m_Materials[0].get());
    m_Shapes.push_back(std::move(floor));

    // �V��
    auto ceiling = std::make_unique<Plane>(
        XMFLOAT3(0.0f, 10.0f, 0.0f),  // �V��̍���
        XMFLOAT3(0.0f, -1.0f, 0.0f),  // �������@��
        10.0f,                         // ��
        10.0f                          // ���s��
    );
    ceiling->SetMaterial(m_Materials[0].get());
    m_Shapes.push_back(std::move(ceiling));

    // ���̕�
    auto backWall = std::make_unique<Plane>(
        XMFLOAT3(0.0f, 5.0f, 10.0f),  // ���̕ǂ̒��S
        XMFLOAT3(0.0f, 0.0f, -1.0f),  // �O�����@��
        10.0f,                         // ��
        10.0f                          // ����
    );
    backWall->SetMaterial(m_Materials[0].get());
    m_Shapes.push_back(std::move(backWall));

    // �E�̕ǁi�ԁj
    auto rightWall = std::make_unique<Plane>(
        XMFLOAT3(5.0f, 5.0f, 5.0f),   // �E�̕ǂ̒��S
        XMFLOAT3(-1.0f, 0.0f, 0.0f),  // �������@��
        10.0f,                         // ���s��
        10.0f                          // ����
    );
    rightWall->SetMaterial(m_Materials[1].get());
    m_Shapes.push_back(std::move(rightWall));

    // ���̕ǁi�΁j
    auto leftWall = std::make_unique<Plane>(
        XMFLOAT3(-5.0f, 5.0f, 5.0f),  // ���̕ǂ̒��S
        XMFLOAT3(1.0f, 0.0f, 0.0f),   // �E�����@��
        10.0f,                         // ���s��
        10.0f                          // ����
    );
    leftWall->SetMaterial(m_Materials[2].get());
    m_Shapes.push_back(std::move(leftWall));

    // �����i�V��̏����Ȏl�p�`�j
    auto light = std::make_unique<Plane>(
        XMFLOAT3(0.0f, 9.99f, 5.0f),  // �V�䂷�ꂷ��
        XMFLOAT3(0.0f, -1.0f, 0.0f),  // �������@��
        2.0f,                          // ��
        2.0f                           // ���s��
    );
    light->SetMaterial(m_Materials[3].get());
    m_Shapes.push_back(std::move(light));

    // �{�b�N�X
    auto box = std::make_unique<Box>(
        XMFLOAT3(-1.5f, 0.0f, 4.0f),  // �ŏ��_
        XMFLOAT3(1.5f, 3.0f, 6.0f)    // �ő�_
    );
    box->SetMaterial(m_Materials[4].get());
    m_Shapes.push_back(std::move(box));

    // ���^����
    auto metalSphere = std::make_unique<Sphere>(
        XMFLOAT3(2.0f, 1.5f, 3.0f),   // ���S
        1.5f                           // ���a
    );
    metalSphere->SetMaterial(m_Materials[5].get());
    m_Shapes.push_back(std::move(metalSphere));

    // �K���X��
    auto glassSphere = std::make_unique<Sphere>(
        XMFLOAT3(-2.0f, 1.5f, 2.0f),  // ���S
        1.5f                           // ���a
    );
    glassSphere->SetMaterial(m_Materials[6].get());
    m_Shapes.push_back(std::move(glassSphere));

    // �V�[���ɃI�u�W�F�N�g��ǉ�
    for ( size_t i = 0; i < m_Shapes.size(); i++ ) {
        AddGameObject<RayTracingObject>(Layer::Gameobject3D, m_Shapes[i].get());
    }
}

void CornellBoxScene::UnInit() {
    // �e�N���X�̉������
    Scene::UnInit();

    // �e���\�[�X�̉��
    m_Shapes.clear();
    m_Materials.clear();
}

void CornellBoxScene::Update() {
    // �e�N���X�̍X�V����
    Scene::Update();
}

void CornellBoxScene::Draw() {
    // �e�N���X�̕`�揈��
    Scene::Draw();
}