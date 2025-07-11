
// CornelBoxScene.cpp
#include "CornelBoxScene.h"
#include "DXRBox.h"
#include "DXRSphere.h"
#include "Input.h"

void CornelBoxScene::Init() {
    CreateMaterials();
    CreateWalls();
    CreateObjects();
    SetupCamera();
}

void CornelBoxScene::Update() {
    DXRScene::Update();
    if( Input::GetKeyPress(VK_LEFT) )
        m_cameraData.position.x -= 5.0f;
    if ( Input::GetKeyPress(VK_RIGHT) )
        m_cameraData.position.x += 5.0f;
    if ( Input::GetKeyPress(VK_UP) )
        m_cameraData.position.y += 5.0f;
    if ( Input::GetKeyPress(VK_DOWN) )
        m_cameraData.position.y -= 5.0f;

    if(Input::GetKeyPress('W') )
		m_cameraData.position.z += 5.0f; // 前進
	if ( Input::GetKeyPress('S') )
		m_cameraData.position.z -= 5.0f; // 後退

}

void CornelBoxScene::CreateMaterials() {
    // シーンが持つユニークなマテリアルのリストをクリア
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
    material.emission = { 1.0f, 1.0f, 1.0f };
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
    // 各オブジェクトにマテリアルデータを直接渡す代わりに、マテリアルID（リストのインデックス）を渡す
    // ※AddGameObjectやDXRBox/SphereのコンストラクタがマテリアルIDを受け取れるように修正が必要です

    // 右壁（緑、ID:1）
    auto* rightWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(10.0f, 555.0f, 555.0f), 1);
    rightWall->SetPosition({ 277.5f, 0.0f, 0.0f });

    // 左壁（赤、ID:0）
    auto* leftWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(10.0f, 555.0f, 555.0f), 0);
    leftWall->SetPosition({ -277.5f, 0.0f, 0.0f });

    // 奥壁（白、ID:2）
    auto* backWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 555.0f, 10.0f), 2);
    backWall->SetPosition({ 0.0f, 0.0f, 0.0f });

    // 床（白、ID:2）
    auto* floor = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 10.0f, 555.0f), 2);
    floor->SetPosition({ 0.0f, -277.5f, 0.0f });

    // 天井（白、ID:2）
    auto* ceiling = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 10.0f, 555.0f), 2);
    ceiling->SetPosition({ 0.0f, 277.5f, 0.0f });

    // 天井ライト（ID:3）
    auto* light = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(130.0f, 5.0f, 105.0f), 3);
    light->SetPosition({ 0.0f, 267.5f, -100.0f });
}

void CornelBoxScene::CreateObjects() {
    // ガラス球（ID:5）
    auto* glassSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 45.0f, 5);
    glassSphere->SetPosition({ 0.0f, -142.5f, -250.0f });

    // アルミニウム球（ID:4）
    auto* aluminumSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 90.0f, 4);
    aluminumSphere->SetPosition({ 150.0f, -107.5f, -125.0f });

    // 白いボックス（ID:2）
    auto* whiteBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(165.0f, 330.5f, 165.0f), 2);
    whiteBox->SetPosition({ -130.0f, -107.5f, -100.0f });
    whiteBox->SetRotation({ 0.0f, XMConvertToRadians(-15.0f), 0.0f });
}

void CornelBoxScene::SetupCamera() {
    CameraData camera;

    // ★ カメラを調整：もう少し近く、視野を広く ★
    camera.position = { 0.0f, 0.0f, -1000.0f };  // Z軸でもう少し近く
    camera.target = { 0.0f, 0.0f, 0.0f };     // シーンの中心を見る
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fov = XMConvertToRadians(60.0f);          // FOVを60度に拡大
    camera.aspect = static_cast<float>( 1920 ) /
        static_cast<float>( 1080 );

    SetCamera(camera);
}