#include "DXRScene.h"
#include "GameManager.h"

// DXRScene基底実装
DXRScene::DXRScene()
    : m_dxrRenderer(nullptr)
    , m_cameraPosition(278.0f, 278.0f, -800.0f)
    , m_cameraTarget(278.0f, 278.0f, 0.0f)
    , m_cameraUp(0.0f, 1.0f, 0.0f)
    , m_fov(40.0f)
    , m_aspect(16.0f / 9.0f)
    , m_maxDepth(50)
    , m_sampleCount(1)
    , m_backgroundColor(0.0f, 0.0f, 0.0f, 1.0f)
    , m_frameIndex(0) {
}

void DXRScene::Init() {
    Scene::Init();
    CreateMaterials();
    BuildCornellBoxScene();
}

void DXRScene::UnInit() {
    Scene::UnInit();
    m_dxrShapes.clear();
}

void DXRScene::Update() {
    Scene::Update();

    // フレーム更新
    m_frameIndex++;

    // カメラ更新
    UpdateCameraData();

    // インスタンス更新
    UpdateInstances();

    // グローバル定数更新
    m_globalConstants.camera = m_camera;
    m_globalConstants.backgroundColor = m_backgroundColor;
    m_globalConstants.maxDepth = m_maxDepth;
    m_globalConstants.sampleCount = m_sampleCount;
    m_globalConstants.frameIndex = m_frameIndex;
}

void DXRScene::Draw() {
    if ( m_dxrRenderer ) {
        // DXRレンダラーにデータ設定
        m_dxrRenderer->SetMaterials(m_materials);
        m_dxrRenderer->SetGlobalConstants(m_globalConstants);

        // レンダリング実行
        m_dxrRenderer->Render();
    }
}

void DXRScene::InitDXR(DXRRenderer* renderer) {
    m_dxrRenderer = renderer;

    if ( m_dxrRenderer ) {
        // ジオメトリデータ収集
        std::vector<std::vector<DXRVertex>> allVertices;
        std::vector<std::vector<uint32_t>> allIndices;

        for ( auto* shape : m_dxrShapes ) {
            allVertices.push_back(shape->GetVertices());
            allIndices.push_back(shape->GetIndices());
        }

        // レンダラーにデータ設定
        m_dxrRenderer->SetGeometries(allVertices, allIndices);
        m_dxrRenderer->SetMaterials(m_materials);
        m_dxrRenderer->SetInstances(m_instances);
        m_dxrRenderer->SetCamera(m_camera);
        m_dxrRenderer->SetGlobalConstants(m_globalConstants);
    }
}

void DXRScene::BuildAccelerationStructures() {
    if ( m_dxrRenderer ) {
        m_dxrRenderer->BuildBLAS();
        m_dxrRenderer->BuildTLAS();
    }
}

void DXRScene::UpdateCamera() {
    UpdateCameraData();
    if ( m_dxrRenderer ) {
        m_dxrRenderer->SetCamera(m_camera);
    }
}

void DXRScene::BuildCornellBoxScene() {
    // 基底クラスでは何もしない（派生クラスで実装）
}

void DXRScene::CreateMaterials() {
    m_materials.clear();

    // CPUレイトレと同じマテリアル設定

    // 0: 赤い壁 (Lambertian)
    DXRMaterial redMaterial;
    redMaterial.albedo = XMFLOAT3(0.65f, 0.05f, 0.05f);
    redMaterial.materialType = 0; // Lambertian
    redMaterial.roughness = 0.0f;
    redMaterial.refractiveIndex = 1.0f;
    redMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(redMaterial);

    // 1: 白い壁 (Lambertian)
    DXRMaterial whiteMaterial;
    whiteMaterial.albedo = XMFLOAT3(0.73f, 0.73f, 0.73f);
    whiteMaterial.materialType = 0; // Lambertian
    whiteMaterial.roughness = 0.0f;
    whiteMaterial.refractiveIndex = 1.0f;
    whiteMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(whiteMaterial);

    // 2: 緑の壁 (Lambertian)
    DXRMaterial greenMaterial;
    greenMaterial.albedo = XMFLOAT3(0.12f, 0.45f, 0.15f);
    greenMaterial.materialType = 0; // Lambertian
    greenMaterial.roughness = 0.0f;
    greenMaterial.refractiveIndex = 1.0f;
    greenMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(greenMaterial);

    // 3: ライト (DiffuseLight)
    DXRMaterial lightMaterial;
    lightMaterial.albedo = XMFLOAT3(1.0f, 1.0f, 1.0f);
    lightMaterial.materialType = 3; // Light
    lightMaterial.roughness = 0.0f;
    lightMaterial.refractiveIndex = 1.0f;
    lightMaterial.emission = XMFLOAT3(15.0f, 15.0f, 15.0f);
    m_materials.push_back(lightMaterial);

    // 4: アルミニウム (Metal)
    DXRMaterial aluminumMaterial;
    aluminumMaterial.albedo = XMFLOAT3(0.8f, 0.85f, 0.88f);
    aluminumMaterial.materialType = 1; // Metal
    aluminumMaterial.roughness = 0.0f;
    aluminumMaterial.refractiveIndex = 1.0f;
    aluminumMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(aluminumMaterial);

    // 5: ガラス (Dielectric)
    DXRMaterial glassMaterial;
    glassMaterial.albedo = XMFLOAT3(1.0f, 1.0f, 1.0f);
    glassMaterial.materialType = 2; // Dielectric
    glassMaterial.roughness = 0.0f;
    glassMaterial.refractiveIndex = 1.5f;
    glassMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(glassMaterial);
}

void DXRScene::UpdateInstances() {
    m_instances.clear();

    // DXRShapeオブジェクトからインスタンスデータを作成
    for ( size_t i = 0; i < m_dxrShapes.size(); i++ ) {
        DXRInstance instance;
        instance.worldMatrix = m_dxrShapes[i]->GetWorldMatrix();
        instance.materialIndex = m_dxrShapes[i]->GetMaterialIndex();
        instance.geometryIndex = static_cast<int>( i );
        m_instances.push_back(instance);
    }
}

void DXRScene::UpdateCameraData() {
    // カメラデータ更新
    m_camera.position = m_cameraPosition;
    m_camera.fov = XMConvertToRadians(m_fov);
    m_camera.aspect = m_aspect;

    // カメラの方向ベクトル計算
    XMVECTOR pos = XMLoadFloat3(&m_cameraPosition);
    XMVECTOR target = XMLoadFloat3(&m_cameraTarget);
    XMVECTOR up = XMLoadFloat3(&m_cameraUp);

    XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, up));
    XMVECTOR cameraUp = XMVector3Cross(right, forward);

    XMStoreFloat3(&m_camera.forward, forward);
    XMStoreFloat3(&m_camera.right, right);
    XMStoreFloat3(&m_camera.up, cameraUp);
}

// CornellBoxDXRScene実装
CornellBoxDXRScene::CornellBoxDXRScene() {
}

void CornellBoxDXRScene::Init() {
    DXRScene::Init();
}

void CornellBoxDXRScene::BuildCornellBoxScene() {
    // CPUレイトレのコーネルボックスシーンを再現

    // 右の壁（緑）
    auto* rightWall = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 555, DXRRect::kYZ);
    rightWall->SetMaterialIndex(2); // 緑

    // 左の壁（赤）
    auto* leftWall = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 0, DXRRect::kYZ);
    leftWall->SetMaterialIndex(0); // 赤

    // 天井のライト
    auto* light = AddDXRGameObject<DXRLight>(Layer::Gameobject3D, 213, 343, 227, 332, 554, DXRRect::kXZ, XMFLOAT3(15.0f, 15.0f, 15.0f));
    light->SetMaterialIndex(3); // ライト

    // 天井（白）
    auto* ceiling = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 555, DXRRect::kXZ);
    ceiling->SetMaterialIndex(1); // 白

    // 床（白）
    auto* floor = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 0, DXRRect::kXZ);
    floor->SetMaterialIndex(1); // 白

    // 奥の壁（白）
    auto* backWall = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 555, DXRRect::kXY);
    backWall->SetMaterialIndex(1); // 白

    // 球体1（アルミニウム）
    auto* sphere1 = AddDXRGameObject<DXRSphere>(Layer::Gameobject3D, 90.0f, 32);
    sphere1->SetPosition(XMFLOAT3(190.0f, 90.0f, 190.0f));
    sphere1->SetMaterialIndex(4); // アルミニウム

    // 球体2（ガラス）
    auto* sphere2 = AddDXRGameObject<DXRSphere>(Layer::Gameobject3D, 45.0f, 32);
    sphere2->SetPosition(XMFLOAT3(380.0f, 45.0f, 100.0f));
    sphere2->SetMaterialIndex(5); // ガラス

    // 箱（白、回転済み）
    auto* box = AddDXRGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(165.0f, 330.0f, 165.0f));
    box->SetPosition(XMFLOAT3(265.0f, 165.0f, 295.0f)); // 中心位置
    box->SetRotation(XMFLOAT3(0.0f, XMConvertToRadians(15.0f), 0.0f)); // Y軸周り15度回転
    box->SetMaterialIndex(1); // 白

    // アスペクト比設定（シーンのサイズに合わせて調整）
    m_aspect = 16.0f / 9.0f; // 適宜調整
}