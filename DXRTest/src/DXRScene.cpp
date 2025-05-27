#include "DXRScene.h"
#include "GameManager.h"

// DXRScene������
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

    // �t���[���X�V
    m_frameIndex++;

    // �J�����X�V
    UpdateCameraData();

    // �C���X�^���X�X�V
    UpdateInstances();

    // �O���[�o���萔�X�V
    m_globalConstants.camera = m_camera;
    m_globalConstants.backgroundColor = m_backgroundColor;
    m_globalConstants.maxDepth = m_maxDepth;
    m_globalConstants.sampleCount = m_sampleCount;
    m_globalConstants.frameIndex = m_frameIndex;
}

void DXRScene::Draw() {
    if ( m_dxrRenderer ) {
        // DXR�����_���[�Ƀf�[�^�ݒ�
        m_dxrRenderer->SetMaterials(m_materials);
        m_dxrRenderer->SetGlobalConstants(m_globalConstants);

        // �����_�����O���s
        m_dxrRenderer->Render();
    }
}

void DXRScene::InitDXR(DXRRenderer* renderer) {
    m_dxrRenderer = renderer;

    if ( m_dxrRenderer ) {
        // �W�I���g���f�[�^���W
        std::vector<std::vector<DXRVertex>> allVertices;
        std::vector<std::vector<uint32_t>> allIndices;

        for ( auto* shape : m_dxrShapes ) {
            allVertices.push_back(shape->GetVertices());
            allIndices.push_back(shape->GetIndices());
        }

        // �����_���[�Ƀf�[�^�ݒ�
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
    // ���N���X�ł͉������Ȃ��i�h���N���X�Ŏ����j
}

void DXRScene::CreateMaterials() {
    m_materials.clear();

    // CPU���C�g���Ɠ����}�e���A���ݒ�

    // 0: �Ԃ��� (Lambertian)
    DXRMaterial redMaterial;
    redMaterial.albedo = XMFLOAT3(0.65f, 0.05f, 0.05f);
    redMaterial.materialType = 0; // Lambertian
    redMaterial.roughness = 0.0f;
    redMaterial.refractiveIndex = 1.0f;
    redMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(redMaterial);

    // 1: ������ (Lambertian)
    DXRMaterial whiteMaterial;
    whiteMaterial.albedo = XMFLOAT3(0.73f, 0.73f, 0.73f);
    whiteMaterial.materialType = 0; // Lambertian
    whiteMaterial.roughness = 0.0f;
    whiteMaterial.refractiveIndex = 1.0f;
    whiteMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(whiteMaterial);

    // 2: �΂̕� (Lambertian)
    DXRMaterial greenMaterial;
    greenMaterial.albedo = XMFLOAT3(0.12f, 0.45f, 0.15f);
    greenMaterial.materialType = 0; // Lambertian
    greenMaterial.roughness = 0.0f;
    greenMaterial.refractiveIndex = 1.0f;
    greenMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(greenMaterial);

    // 3: ���C�g (DiffuseLight)
    DXRMaterial lightMaterial;
    lightMaterial.albedo = XMFLOAT3(1.0f, 1.0f, 1.0f);
    lightMaterial.materialType = 3; // Light
    lightMaterial.roughness = 0.0f;
    lightMaterial.refractiveIndex = 1.0f;
    lightMaterial.emission = XMFLOAT3(15.0f, 15.0f, 15.0f);
    m_materials.push_back(lightMaterial);

    // 4: �A���~�j�E�� (Metal)
    DXRMaterial aluminumMaterial;
    aluminumMaterial.albedo = XMFLOAT3(0.8f, 0.85f, 0.88f);
    aluminumMaterial.materialType = 1; // Metal
    aluminumMaterial.roughness = 0.0f;
    aluminumMaterial.refractiveIndex = 1.0f;
    aluminumMaterial.emission = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_materials.push_back(aluminumMaterial);

    // 5: �K���X (Dielectric)
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

    // DXRShape�I�u�W�F�N�g����C���X�^���X�f�[�^���쐬
    for ( size_t i = 0; i < m_dxrShapes.size(); i++ ) {
        DXRInstance instance;
        instance.worldMatrix = m_dxrShapes[i]->GetWorldMatrix();
        instance.materialIndex = m_dxrShapes[i]->GetMaterialIndex();
        instance.geometryIndex = static_cast<int>( i );
        m_instances.push_back(instance);
    }
}

void DXRScene::UpdateCameraData() {
    // �J�����f�[�^�X�V
    m_camera.position = m_cameraPosition;
    m_camera.fov = XMConvertToRadians(m_fov);
    m_camera.aspect = m_aspect;

    // �J�����̕����x�N�g���v�Z
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

// CornellBoxDXRScene����
CornellBoxDXRScene::CornellBoxDXRScene() {
}

void CornellBoxDXRScene::Init() {
    DXRScene::Init();
}

void CornellBoxDXRScene::BuildCornellBoxScene() {
    // CPU���C�g���̃R�[�l���{�b�N�X�V�[�����Č�

    // �E�̕ǁi�΁j
    auto* rightWall = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 555, DXRRect::kYZ);
    rightWall->SetMaterialIndex(2); // ��

    // ���̕ǁi�ԁj
    auto* leftWall = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 0, DXRRect::kYZ);
    leftWall->SetMaterialIndex(0); // ��

    // �V��̃��C�g
    auto* light = AddDXRGameObject<DXRLight>(Layer::Gameobject3D, 213, 343, 227, 332, 554, DXRRect::kXZ, XMFLOAT3(15.0f, 15.0f, 15.0f));
    light->SetMaterialIndex(3); // ���C�g

    // �V��i���j
    auto* ceiling = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 555, DXRRect::kXZ);
    ceiling->SetMaterialIndex(1); // ��

    // ���i���j
    auto* floor = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 0, DXRRect::kXZ);
    floor->SetMaterialIndex(1); // ��

    // ���̕ǁi���j
    auto* backWall = AddDXRGameObject<DXRRect>(Layer::Gameobject3D, 0, 555, 0, 555, 555, DXRRect::kXY);
    backWall->SetMaterialIndex(1); // ��

    // ����1�i�A���~�j�E���j
    auto* sphere1 = AddDXRGameObject<DXRSphere>(Layer::Gameobject3D, 90.0f, 32);
    sphere1->SetPosition(XMFLOAT3(190.0f, 90.0f, 190.0f));
    sphere1->SetMaterialIndex(4); // �A���~�j�E��

    // ����2�i�K���X�j
    auto* sphere2 = AddDXRGameObject<DXRSphere>(Layer::Gameobject3D, 45.0f, 32);
    sphere2->SetPosition(XMFLOAT3(380.0f, 45.0f, 100.0f));
    sphere2->SetMaterialIndex(5); // �K���X

    // ���i���A��]�ς݁j
    auto* box = AddDXRGameObject<DXRBox>(Layer::Gameobject3D, XMFLOAT3(165.0f, 330.0f, 165.0f));
    box->SetPosition(XMFLOAT3(265.0f, 165.0f, 295.0f)); // ���S�ʒu
    box->SetRotation(XMFLOAT3(0.0f, XMConvertToRadians(15.0f), 0.0f)); // Y������15�x��]
    box->SetMaterialIndex(1); // ��

    // �A�X�y�N�g��ݒ�i�V�[���̃T�C�Y�ɍ��킹�Ē����j
    m_aspect = 16.0f / 9.0f; // �K�X����
}