
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
    // 赤い壁（左壁）
    m_redMaterial.albedo = { 0.65f, 0.05f, 0.05f };
    m_redMaterial.roughness = 1.0f;
    m_redMaterial.refractiveIndex = 1.0f;
    m_redMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_redMaterial.materialType = 0; // Lambertian

    // 緑の壁（右壁）
    m_greenMaterial.albedo = { 0.12f, 0.45f, 0.15f };
    m_greenMaterial.roughness = 1.0f;
    m_greenMaterial.refractiveIndex = 1.0f;  // ★ 修正済み ★
    m_greenMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_greenMaterial.materialType = 0; // Lambertian

    // 白い壁
    m_whiteMaterial.albedo = { 0.73f, 0.73f, 0.73f };
    m_whiteMaterial.roughness = 1.0f;
    m_whiteMaterial.refractiveIndex = 1.0f;  // ★ 修正済み ★
    m_whiteMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_whiteMaterial.materialType = 0; // Lambertian

    // 発光マテリアル（天井ライト）
    m_lightMaterial.albedo = { 1.0f, 1.0f, 1.0f };
    m_lightMaterial.roughness = 0.0f;
    m_lightMaterial.refractiveIndex = 1.0f;
    m_lightMaterial.emission = { 15.0f, 15.0f, 15.0f };
    m_lightMaterial.materialType = 3; // DiffuseLight

    // 金属マテリアル（アルミニウム球）
    m_metalMaterial.albedo = { 0.8f, 0.85f, 0.88f };
    m_metalMaterial.roughness = 0.0f;
    m_metalMaterial.refractiveIndex = 1.0f;
    m_metalMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_metalMaterial.materialType = 1; // Metal

    // ガラスマテリアル（誘電体球）
    m_glassMaterial.albedo = { 1.0f, 1.0f, 1.0f };
    m_glassMaterial.roughness = 0.0f;
    m_glassMaterial.refractiveIndex = 1.5f;
    m_glassMaterial.emission = { 0.0f, 0.0f, 0.0f };
    m_glassMaterial.materialType = 2; // Dielectric
}

void CornelBoxScene::CreateWalls() {
    // 右壁（緑）
    auto* leftWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3( 10.0f, 555.0f, 555.0f ), m_redMaterial);
    leftWall->SetPosition({ 277.5f, 0.0f, 0.0f });

    // 左壁（赤）
    auto* rightWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(10.0f, 555.0f, 555.0f), m_greenMaterial);
    rightWall->SetPosition({ -277.5f, 0.0f, 0.0f });

    // 奥壁（白）
    auto* backWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 555.0f, 10.0f), m_whiteMaterial);
    backWall->SetPosition({ 0.0f, 0.0f, 0.0f });
    
    // 床（白）
    auto* floor = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 10.0f, 555.0f), m_whiteMaterial);
    floor->SetPosition({ 0.0f, -277.5f, 0.0f });

    // 天井（白）
    auto* ceiling = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(555.0f, 10.0f, 555.0f), m_whiteMaterial);
    ceiling->SetPosition({ 0.0f, 277.5f, 0.0f });

    // 天井ライト
    auto* light = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(130.0f, 5.0f, 105.0f), m_lightMaterial);
    light->SetPosition({ 0.0f, 267.5f, -100.0f });
}

void CornelBoxScene::CreateObjects() {
    // アルミニウム球（少し大きく）
    auto* aluminumSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 12.0f, m_metalMaterial);
    aluminumSphere->SetPosition({ 0.0f, 190.0f, 300.0f }); // 手前に

    // ガラス球（少し大きく）
    auto* glassSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 8.0f, m_glassMaterial);
    glassSphere->SetPosition({ 380.0f, 190.0f, 250.0f }); // 手前に

    // 白いボックス（大きく）
    auto* whiteBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(165.0f, 330.5f, 165.0f), m_whiteMaterial);
    whiteBox->SetPosition({ -130.0f, -107.5f, -100.0f }); // 中央、手前に
    whiteBox->SetRotation({ 0.0f, XMConvertToRadians(-15.0f), 0.0f }); // 45度回転
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