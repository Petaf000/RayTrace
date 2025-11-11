
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
		m_cameraData.position.z += CAMERA_SPEED;
	if ( Input::GetKeyPress('S') )
		m_cameraData.position.z -= CAMERA_SPEED;

}

void CornelBoxScene::CreateMaterials() {
    m_uniqueMaterials.clear();

    DXRMaterialData material;

    material.albedo = { 0.65f, 0.05f, 0.05f };
    material.roughness = 1.0f;
    material.refractiveIndex = 1.0f;
    material.emission = { 0.0f, 0.0f, 0.0f };
    material.materialType = 0;
    m_uniqueMaterials.push_back(material);

    material.albedo = { 0.12f, 0.45f, 0.15f };
    material.materialType = 0;
    m_uniqueMaterials.push_back(material);

    material.albedo = { 0.73f, 0.73f, 0.73f };
    material.materialType = 0;
    m_uniqueMaterials.push_back(material);
    
    material.albedo = { 1.0f, 1.0f, 1.0f };
    material.emission = { 18.4f, 15.6f, 11.2f };
    material.materialType = 3;
    m_uniqueMaterials.push_back(material);

    material.albedo = { 0.8f, 0.85f, 0.88f };
    material.roughness = 0.0f;
    material.emission = { 0.0f, 0.0f, 0.0f };
    material.materialType = 1;
    m_uniqueMaterials.push_back(material);

    material.albedo = { 1.0f, 1.0f, 1.0f };
    material.roughness = 0.0f;
    material.refractiveIndex = 2.0f;
    material.materialType = 2;
    m_uniqueMaterials.push_back(material);
}

void CornelBoxScene::CreateWalls() {
    auto* rightWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.02f, 2.0f, 2.0f), 1);
    rightWall->SetPosition({ 0.99f, 1.0f, 0.0f });

    auto* leftWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.02f, 2.0f, 2.0f), 0);
    leftWall->SetPosition({ -0.99f, 1.0f, 0.0f });

    auto* backWall = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(2.0f, 2.0f, 0.02f), 2);
    backWall->SetPosition({ 0.0f, 1.0f, 1.0f });

    auto* floor = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(2.0f, 0.02f, 2.0f), 2);
    floor->SetPosition({ 0.0f, 0.0f, 0.0f });

    auto* ceiling = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(2.0f, 0.02f, 2.0f), 2);
    ceiling->SetPosition({ 0.0f, 2.0f, 0.0f });

    auto* light = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.47f, 0.01f, 0.38f), 3);
    light->SetPosition({ 0.0f, 1.98f, 0.0f });
}

void CornelBoxScene::CreateObjects() {
    /*
    auto* shortBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.6f, 0.6f, 0.6f), 2);
    shortBox->SetPosition({ 0.32f, 0.3f, -0.37f });
    shortBox->SetRotation({ 0.0f, XMConvertToRadians(17.0f), 0.0f });
    */
    auto* tallBox = AddGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(0.6f, 1.2f, 0.6f), 2);
    tallBox->SetPosition({ -0.3f, 0.6f, 0.3f });
    tallBox->SetRotation({ 0.0f, XMConvertToRadians(-18.0f), 0.0f });
    
    auto* glassSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 0.3f, 5);
    glassSphere->SetPosition({ 0.32f, 0.3f, -0.37f });
    /*
    auto* aluminumSphere = AddGameObject<DXRSphere>(Layer::Gameobject3D, 90.0f, 4);
    aluminumSphere->SetPosition({ 150.0f, -107.5f, -125.0f });
    */
}

void CornelBoxScene::SetupCamera() {
    CameraData camera;

    camera.position = { 0.0f, 1.0f, -3.0f };
    camera.target = { 0.0f, 1.0f, 1.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fov = XMConvertToRadians(60.0f);
    camera.aspect = static_cast<float>( 1920 ) /
        static_cast<float>( 1080 );

    SetCamera(camera);
}