
// CornelBoxScene.cpp
#include "CornelBoxScene.h"
#include "DXRBox.h"
#include "DXRSphere.h"
#include "Input.h"

#define CAMERA_SPEED 0.01f

void CornelBoxScene::Init() {
    CreateMaterials();
    CreateWalls();
    CreateObjects();
    SetupCamera();
}

void CornelBoxScene::Update() {
    DXRScene::Update();
    if( Input::GetKeyPress(VK_LEFT) )
        m_cameraData.position.x -= CAMERA_SPEED;
    if ( Input::GetKeyPress(VK_RIGHT) )
        m_cameraData.position.x += CAMERA_SPEED;
    if ( Input::GetKeyPress(VK_UP) )
        m_cameraData.position.y += CAMERA_SPEED;
    if ( Input::GetKeyPress(VK_DOWN) )
        m_cameraData.position.y -= CAMERA_SPEED;

    if(Input::GetKeyPress('W') )
		m_cameraData.position.z += CAMERA_SPEED; // �O�i
	if ( Input::GetKeyPress('S') )
		m_cameraData.position.z -= CAMERA_SPEED; // ���

}

void CornelBoxScene::CreateMaterials() {
    // �V�[���������j�[�N�ȃ}�e���A���̃��X�g���N���A
    m_uniqueMaterials.clear();

    DXRMaterialData material;

    // Material 0: Red
    material.albedo = { 0.65f, 0.05f, 0.05f };
    material.roughness = 1.0f;
    material.refractiveIndex = 1.0f;
    material.emission = { 0.0f, 0.0f, 0.0f };
    material.materialType = 0; // Lambertian
    m_uniqueMaterials.push_back(material);

    // Material 1: Green
    material.albedo = { 0.12f, 0.45f, 0.15f };
    material.materialType = 0; // Lambertian
    m_uniqueMaterials.push_back(material);

    // Material 2: White
    material.albedo = { 0.73f, 0.73f, 0.73f };
    material.materialType = 0; // Lambertian
    m_uniqueMaterials.push_back(material);
    
    // Material 3: Light
    material.albedo = { 1.0f, 1.0f, 1.0f };
    material.emission = { 17 / 1.5, 12 / 1.5, 4 / 1.5 }; // 物理的に適切な発光強度（W/m²sr）
    material.materialType = 3; // DiffuseLight
    m_uniqueMaterials.push_back(material);

    // Material 4: Metal
    material.albedo = { 0.8f, 0.85f, 0.88f };
    material.roughness = 0.0f;
    material.emission = { 0.0f, 0.0f, 0.0f };
    material.materialType = 1; // Metal
    m_uniqueMaterials.push_back(material);

    // Material 5: Glass
    material.albedo = { 1.0f, 1.0f, 1.0f };
    material.roughness = 0.0f;
    material.refractiveIndex = 1.5f;
    material.materialType = 2; // Dielectric
    m_uniqueMaterials.push_back(material);
}

void CornelBoxScene::CreateWalls() {
    // �e�I�u�W�F�N�g�Ƀ}�e���A���f�[�^�𒼐ړn������ɁA�}�e���A��ID�i���X�g�̃C���f�b�N�X�j��n��
    // ��AddGameObject��DXRBox/Sphere�̃R���X�g���N�^���}�e���A��ID���󂯎���悤�ɏC�����K�v�ł�

    // **物理的に正確な2m×2m Cornell Box（床を原点基準）**
    
    // 右壁（緑、ID:1）- 厚さ0.02m
    auto* rightWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.02f, 2.0f, 2.0f), 1);
    rightWall->SetPosition({ 0.99f, 1.0f, 0.0f });

    // 左壁（赤、ID:0）- 厚さ0.02m
    auto* leftWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.02f, 2.0f, 2.0f), 0);
    leftWall->SetPosition({ -0.99f, 1.0f, 0.0f });

    // 奥壁（白、ID:2）- 厚さ0.02m
    auto* backWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(2.0f, 2.0f, 0.02f), 2);
    backWall->SetPosition({ 0.0f, 1.0f, 1.0f });

    // 床（白、ID:2）- 厚さ0.02m、Y=0が床面
    auto* floor = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(2.0f, 0.02f, 2.0f), 2);
    floor->SetPosition({ 0.0f, 0.0f, 0.0f });

    // 天井（白、ID:2）- 厚さ0.02m
    auto* ceiling = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(2.0f, 0.02f, 2.0f), 2);
    ceiling->SetPosition({ 0.0f, 2.0f, 0.0f });

    // 天井ライト（ID:3）- 物理的に適切なサイズ 0.3m×0.25m
    auto* light = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.47f, 0.01f, 0.38f), 3);
    light->SetPosition({ 0.0f, 1.98f, 0.0f });
}

void CornelBoxScene::CreateObjects() {
    /*
    // �����{�b�N�X�iID:2�j
    auto* shortBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(166.5, 166.5, 166.5), 2);
    shortBox->SetPosition({ 88.8, 83.25, 102.675 });
    shortBox->SetRotation({ 0.0f, XMConvertToRadians(-18.0f), 0.0f });

    auto* tallBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(166.5, 333.0, 166.5), 2);
    tallBox->SetPosition({ -91.575, 166.5, -80.475 });
    tallBox->SetRotation({ 0.0f, XMConvertToRadians(15.0f), 0.0f });
    */
    // **物理的スケール（メートル単位）のオブジェクト**
    
    // 短いボックス - 30cm×30cm×30cm
    auto* shortBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.6f, 0.6f, 0.6f), 2);
    shortBox->SetPosition({ 0.32f, 0.3f, -0.37f }); // 床から15cm上
    shortBox->SetRotation({ 0.0f, XMConvertToRadians(17.0f), 0.0f });

    // 高いボックス - 30cm×60cm×30cm  
    auto* tallBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.6f, 1.2f, 0.6f), 2);
    tallBox->SetPosition({ -0.3f, 0.6f, 0.3f }); // 床から30cm上
    tallBox->SetRotation({ 0.0f, XMConvertToRadians(-18.0f), 0.0f });

/*
    // �K���X���iID:5�j
    auto* glassSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 45.0f, 5);
    glassSphere->SetPosition({ 0.0f, -142.5f, -250.0f });

    // �A���~�j�E�����iID:4�j
    auto* aluminumSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 90.0f, 4);
    aluminumSphere->SetPosition({ 150.0f, -107.5f, -125.0f });
    */
}

void CornelBoxScene::SetupCamera() {
    CameraData camera;

    // **物理的スケール用カメラ設定（2m Cornell Box）**
    camera.position = { 0.0f, 1.0f, -3.0f };  // 床から1m、手前0.8m
    camera.target = { 0.0f, 1.0f, 1.0f };     // 部屋の中央を注視
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fov = XMConvertToRadians(60.0f);    // 60度FOV（標準）
    camera.aspect = static_cast<float>( 1920 ) /
        static_cast<float>( 1080 );

    SetCamera(camera);
}