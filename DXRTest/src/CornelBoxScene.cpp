
// CornelBoxScene.cpp
#include "CornelBoxScene.h"
#include "DXRBox.h"
#include "DXRSphere.h"

void CornelBoxScene::Init() {
    CreateMaterials();
    CreateWalls();
    CreateObjects();
    SetupCamera();
}

void CornelBoxScene::CreateMaterials() {
    // �Ԃ��ǁi���ǁj- materialType = 0
    m_redMaterial.albedo = { 0.65f, 0.05f, 0.05f };
    m_redMaterial.roughness = 1.0f;
    m_redMaterial.refractiveIndex = 1.0f;
    m_redMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_redMaterial.materialType = 0; // Lambertian - �ԗp

    // �΂̕ǁi�E�ǁj- materialType = 1�i�ꎞ�I��Metal�Ƃ��Ĉ����j
    m_greenMaterial.albedo = { 0.12f, 0.45f, 0.15f };
    m_greenMaterial.roughness = 1.0f;
    m_greenMaterial.refractiveIndex = 1.0f;
    m_greenMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_greenMaterial.materialType = 0; // �ꎞ�I��1�Ԃ��g�p

    // ������ - materialType = 2�i�ꎞ�I��Dielectric�Ƃ��Ĉ����j
    m_whiteMaterial.albedo = { 0.73f, 0.73f, 0.73f };
    m_whiteMaterial.roughness = 1.0f;
    m_whiteMaterial.refractiveIndex = 1.0f;
    m_whiteMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_whiteMaterial.materialType = 0; // �ꎞ�I��2�Ԃ��g�p

    // �����}�e���A���i�V�䃉�C�g�j
    m_lightMaterial.albedo = { 1.0f, 1.0f, 1.0f };
    m_lightMaterial.roughness = 0.0f;
    m_lightMaterial.refractiveIndex = 1.0f;
    m_lightMaterial.emission = { 15.0f, 15.0f, 15.0f };
    m_lightMaterial.materialType = 3; // DiffuseLight

    // �����}�e���A���i�A���~�j�E�����j
    m_metalMaterial.albedo = { 0.8f, 0.85f, 0.88f };
    m_metalMaterial.roughness = 0.0f;
    m_metalMaterial.refractiveIndex = 1.0f;
    m_metalMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_metalMaterial.materialType = 1; // Metal�i���ۂ�Metal�j

    // �K���X�}�e���A���i�U�d�̋��j
    m_glassMaterial.albedo = { 1.0f, 1.0f, 1.0f };
    m_glassMaterial.roughness = 0.0f;
    m_glassMaterial.refractiveIndex = 1.5f;
    m_glassMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_glassMaterial.materialType = 2; // Dielectric�i���ۂ�Dielectric�j
}

void CornelBoxScene::CreateWalls() {
    // �E�ǁi�΁j
    auto* leftWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(10.0f, 555.0f, 555.0f), m_greenMaterial);
    leftWall->SetPosition({ 277.5f, 0.0f, 0.0f });

    // ���ǁi�ԁj
    auto* rightWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(10.0f, 555.0f, 555.0f), m_redMaterial);
    rightWall->SetPosition({ -277.5f, 0.0f, 0.0f });

    // ���ǁi���j
    auto* backWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 555.0f, 10.0f), m_whiteMaterial);
    backWall->SetPosition({ 0.0f, 0.0f, 0.0f });

    // ���i���j
    auto* floor = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 10.0f, 555.0f), m_whiteMaterial);
    floor->SetPosition({ 0.0f, -277.5f, 0.0f });

    // �V��i���j
    auto* ceiling = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 10.0f, 555.0f), m_whiteMaterial);
    ceiling->SetPosition({ 0.0f, 277.5f, 0.0f });

    // �V�䃉�C�g
    auto* light = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(130.0f, 5.0f, 105.0f), m_lightMaterial);
    light->SetPosition({ 0.0f, 267.5f, -100.0f });
}

void CornelBoxScene::CreateObjects() {
   
    // �K���X���i�����傫���j
    auto* glassSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 45.0f, m_glassMaterial);
    glassSphere->SetPosition({ 0.0f, -102.5f, -250.0f }); // ��O��

    // �A���~�j�E�����i�����傫���j
    auto* aluminumSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 90.0f, m_metalMaterial);
    aluminumSphere->SetPosition({ 150.0f, -107.5f, -125.0f }); // ��O��
    // �����{�b�N�X�i�傫���j
    auto* whiteBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(165.0f, 330.5f, 165.0f), m_whiteMaterial);
    whiteBox->SetPosition({ -130.0f, -107.5f, -100.0f }); // �����A��O��
    whiteBox->SetRotation({ 0.0f, XMConvertToRadians(-15.0f), 0.0f }); // 45�x��]

    
}

void CornelBoxScene::SetupCamera() {
    CameraData camera;

    // �� �J�����𒲐��F���������߂��A������L�� ��
    camera.position = { 0.0f, 0.0f, -1000.0f };  // Z���ł��������߂�
    camera.target = { 0.0f, 0.0f, 0.0f };     // �V�[���̒��S������
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fov = XMConvertToRadians(60.0f);          // FOV��60�x�Ɋg��
    camera.aspect = static_cast<float>( 1920 ) /
        static_cast<float>( 1080 );

    SetCamera(camera);
}