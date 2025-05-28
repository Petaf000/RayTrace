
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
    // �Ԃ��ǁi���ǁj
    m_redMaterial.albedo = { 0.65f, 0.05f, 0.05f };
    m_redMaterial.roughness = 1.0f;
    m_redMaterial.refractiveIndex = 1.0f;
    m_redMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_redMaterial.materialType = 0; // Lambertian

    // �΂̕ǁi�E�ǁj
    m_greenMaterial.albedo = { 0.12f, 0.45f, 0.15f };
    m_greenMaterial.roughness = 1.0f;
    m_greenMaterial.refractiveIndex = 1.0f;
    m_greenMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_greenMaterial.materialType = 0; // Lambertian

    // ������
    m_whiteMaterial.albedo = { 0.73f, 0.73f, 0.73f };
    m_whiteMaterial.roughness = 1.0f;
    m_greenMaterial.refractiveIndex = 1.0f;
    m_whiteMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_whiteMaterial.materialType = 0; // Lambertian

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
    m_metalMaterial.materialType = 1; // Metal

    // �K���X�}�e���A���i�U�d�̋��j
    m_glassMaterial.albedo = { 1.0f, 1.0f, 1.0f };
    m_glassMaterial.roughness = 0.0f;
    m_glassMaterial.refractiveIndex = 1.5f;
    m_glassMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_glassMaterial.materialType = 2; // Dielectric
}

void CornelBoxScene::CreateWalls() {
    // ���ǁi�ԁj
    auto* leftWall = AddGameObject<DXRBox>(Layer::Gameobject3D);
    leftWall->SetSize({ 10.0f, 555.0f, 555.0f });
    leftWall->SetPosition({ -277.5f, 277.5f, 277.5f });
    leftWall->SetMaterialData(m_redMaterial);

    // �E�ǁi�΁j
    auto* rightWall = AddGameObject<DXRBox>(Layer::Gameobject3D);
    rightWall->SetSize({ 10.0f, 555.0f, 555.0f });
    rightWall->SetPosition({ 277.5f, 277.5f, 277.5f });
    rightWall->SetMaterialData(m_greenMaterial);

    // ���ǁi���j
    auto* backWall = AddGameObject<DXRBox>(Layer::Gameobject3D);
    backWall->SetSize({ 555.0f, 555.0f, 10.0f });
    backWall->SetPosition({ 0.0f, 277.5f, 277.5f });
    backWall->SetMaterialData(m_whiteMaterial);

    // ���i���j
    auto* floor = AddGameObject<DXRBox>(Layer::Gameobject3D);
    floor->SetSize({ 555.0f, 10.0f, 555.0f });
    floor->SetPosition({ 0.0f, -5.0f, 277.5f });
    floor->SetMaterialData(m_whiteMaterial);

    // �V��i���j
    auto* ceiling = AddGameObject<DXRBox>(Layer::Gameobject3D);
    ceiling->SetSize({ 555.0f, 10.0f, 555.0f });
    ceiling->SetPosition({ 0.0f, 560.0f, 277.5f });
    ceiling->SetMaterialData(m_whiteMaterial);

    // �V�䃉�C�g
    auto* light = AddGameObject<DXRBox>(Layer::Gameobject3D);
    light->SetSize({ 130.0f, 5.0f, 105.0f });
    light->SetPosition({ 0.0f, 554.0f, 279.5f });
    light->SetMaterialData(m_lightMaterial);
}

void CornelBoxScene::CreateObjects() {
    // �A���~�j�E����
    auto* aluminumSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D);
    aluminumSphere->SetRadius(90.0f);
    aluminumSphere->SetPosition({ 190.0f, 90.0f, 190.0f });
	aluminumSphere->SetMaterialData(m_metalMaterial);

    // �K���X��
    auto* glassSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D);
    glassSphere->SetRadius(45.0f);
    glassSphere->SetPosition({ 380.0f, 45.0f, 100.0f });
    glassSphere->SetMaterialData(m_glassMaterial);

    // �����{�b�N�X�i��]�j
    auto* whiteBox = AddGameObject<DXRBox>(Layer::Gameobject3D);
    whiteBox->SetSize({ 165.0f, 330.0f, 165.0f });
    whiteBox->SetPosition({ 265.0f, 165.0f, 295.0f });
    // 15�x��]�iY������j
    whiteBox->SetRotation({ 0.0f, XMConvertToRadians(15.0f), 0.0f });
    whiteBox->SetMaterialData(m_whiteMaterial);
}

void CornelBoxScene::SetupCamera() {
    CameraData camera;
    camera.position = { 278.0f, 278.0f, -800.0f };
    camera.target = { 278.0f, 278.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fov = XMConvertToRadians(40.0f);
    camera.aspect = 16.0f / 9.0f; // �f�t�H���g�l�A���ۂ̃A�X�y�N�g��͎��s���ɐݒ�

    SetCamera(camera);
}