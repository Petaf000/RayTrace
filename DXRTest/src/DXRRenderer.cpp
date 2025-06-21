// DXRRenderer.cpp
#include "DXRRenderer.h"
#include "Renderer.h"
#include "GameManager.h"
#include "DXRScene.h"
#include "App.h"

void DXRRenderer::Init(Renderer* renderer) {
    // Renderer����device�擾
    ID3D12Device* baseDevice = renderer->GetDevice();
    HRESULT hr = baseDevice->QueryInterface(IID_PPV_ARGS(&m_device));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create DXR Device");
    }

    // CommandQueue��CommandList�擾
    m_commandQueue = renderer->GetCommandQueue();

    ComPtr<ID3D12GraphicsCommandList> baseCommandList = renderer->GetCommandList();
    hr = baseCommandList->QueryInterface(IID_PPV_ARGS(&m_commandList));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create DXR CommandList");
    }

    // �T�C�Y��Renderer�Ɗ��S�Ɉ�v������
    m_width = renderer->GetBufferWidth();
    m_height = renderer->GetBufferHeight();

    char debugMsg[256];
    sprintf_s(debugMsg, "=== DXR Init ===\n");
    OutputDebugStringA(debugMsg);
    sprintf_s(debugMsg, "Setting DXR size to match renderer: %ux%u\n", m_width, m_height);
    OutputDebugStringA(debugMsg);

    InitializeDXR(m_device.Get());
    CreateRootSignature();
    CreateRaytracingPipelineStateObject();
    CreateOutputResource();
    CreateAccelerationStructures();
    CreateShaderTables();

    // �� ImGui�������͍폜�iRenderer���Ǘ��j ��
    // ImGui�������R�[�h���폜

    OutputDebugStringA("DXR initialization completed\n");
}

void DXRRenderer::UnInit() {
    // ���\�[�X�̃N���[���A�b�v
    m_rayGenShaderTable.Reset();
    m_missShaderTable.Reset();
    m_hitGroupShaderTable.Reset();
    m_raytracingOutput.Reset();
    m_descriptorHeap.Reset();
    m_topLevelAS.Reset();
    m_topLevelASScratch.Reset();
    m_bottomLevelAS.clear();
    m_bottomLevelASScratch.clear();
    m_rtStateObject.Reset();
    m_globalRootSignature.Reset();
    m_sceneConstantBuffer.Reset();
}

void DXRRenderer::Render() {
    static int frameCount = 0;
    frameCount++;

    char debugMsg[512];
    sprintf_s(debugMsg, "=== Frame %d - Detailed Debug ===\n", frameCount);
    OutputDebugStringA(debugMsg);

    // ���݂̃V�[�����擾
    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );

    if ( !dxrScene ) {
        OutputDebugStringA("ERROR: No DXRScene found!\n");
        return;
    }

    UpdateCamera();

    // �� �T�C�Y�̊m�F�ƏC�� ��
    auto& renderer = Singleton<Renderer>::getInstance();
    ID3D12Resource* currentBackBuffer = renderer.GetBackBuffer(renderer.GetCurrentFrameIndex());

    if ( !currentBackBuffer ) {
        OutputDebugStringA("ERROR: currentBackBuffer is NULL!\n");
        return;
    }

    // �o�b�N�o�b�t�@�̃��\�[�X�L�q���擾
    D3D12_RESOURCE_DESC backBufferDesc = currentBackBuffer->GetDesc();

    sprintf_s(debugMsg, "BackBuffer: %ux%u (format: %d)\n",
        (UINT)backBufferDesc.Width, backBufferDesc.Height, (int)backBufferDesc.Format);
    OutputDebugStringA(debugMsg);
    sprintf_s(debugMsg, "DXR current size: %ux%u\n", m_width, m_height);
    OutputDebugStringA(debugMsg);

    // �� 4�t���[���ڈȍ~�F���C�g���[�V���O���s ��
    OutputDebugStringA("=== Raytracing Execution ===\n");

    // ���C�g���[�V���O���s
    D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};

    // RayGeneration shader
    raytraceDesc.RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = s_shaderTableEntrySize;

    // Miss shader
    raytraceDesc.MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
    raytraceDesc.MissShaderTable.SizeInBytes = s_shaderTableEntrySize;
    raytraceDesc.MissShaderTable.StrideInBytes = s_shaderTableEntrySize;

    // Hit group
    raytraceDesc.HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
    raytraceDesc.HitGroupTable.SizeInBytes = s_hitGroupEntrySize * 4;  // 4�̃}�e���A���^�C�v��
    raytraceDesc.HitGroupTable.StrideInBytes = s_hitGroupEntrySize;
    /*
    raytraceDesc.HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
    raytraceDesc.HitGroupTable.SizeInBytes = s_hitGroupEntrySize;
    raytraceDesc.HitGroupTable.StrideInBytes = s_hitGroupEntrySize;
    */
    // �� �f�B�X�p�b�`�ݒ�i�T�C�Y�m�F�ς݁j ��
    raytraceDesc.Width = m_width;
    raytraceDesc.Height = m_height;
    raytraceDesc.Depth = 1;

    sprintf_s(debugMsg, "About to dispatch rays: %ux%ux%u\n",
        raytraceDesc.Width, raytraceDesc.Height, raytraceDesc.Depth);
    OutputDebugStringA(debugMsg);


    // �O���[�o�����[�g�V�O�l�`���ݒ�
    m_commandList->SetComputeRootSignature(m_globalRootSignature.Get());

    // DXR�p�f�B�X�N���v�^�q�[�v�ݒ�
    ID3D12DescriptorHeap* dxrHeaps[] = { m_descriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, dxrHeaps);

    // ���\�[�X�ݒ�
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->SetComputeRootDescriptorTable(0, srvHandle);  // UAV (�o��)
    m_commandList->SetComputeRootShaderResourceView(1, m_topLevelAS->GetGPUVirtualAddress());  // TLAS
    m_commandList->SetComputeRootConstantBufferView(2, m_sceneConstantBuffer->GetGPUVirtualAddress());  // �萔�o�b�t�@

    // ������ �V�K�ǉ��F�C���X�^���X/���_/�C���f�b�N�X�o�b�t�@ ������
    srvHandle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_commandList->SetComputeRootDescriptorTable(3, srvHandle);  // SRV�e�[�u�� (t1-t3)

    // ���C�g���[�V���O���s
    m_commandList->SetPipelineState1(m_rtStateObject.Get());

    OutputDebugStringA("Executing DispatchRays...\n");
    m_commandList->DispatchRays(&raytraceDesc);
    OutputDebugStringA("DispatchRays completed successfully!\n");

    // UAV�o���A
    CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_raytracingOutput.Get());
    m_commandList->ResourceBarrier(1, &uavBarrier);

    // �� ���C�g���[�V���O���ʂ��o�b�N�o�b�t�@�ɃR�s�[ ��
    OutputDebugStringA("Starting raytracing result copy...\n");

    // ���\�[�X��Ԃ�J��
    CD3DX12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
    };
    m_commandList->ResourceBarrier(2, barriers);

    // ���\�[�X�R�s�[
    m_commandList->CopyResource(currentBackBuffer, m_raytracingOutput.Get());
    OutputDebugStringA("CopyResource completed\n");

    // ���\�[�X��Ԃ𕜌�
    CD3DX12_RESOURCE_BARRIER restoreBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT)
    };
    m_commandList->ResourceBarrier(2, restoreBarriers);

    OutputDebugStringA("Raytracing render completed successfully!\n");
}

void DXRRenderer::InitializeDXR(ID3D12Device* device) {
    // DXR�@�\�̊m�F
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));

    if ( FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED ) {
        throw std::runtime_error("DXR is not supported on this device");
    }
}

void DXRRenderer::CreateRootSignature() {
    // �O���[�o�����[�g�V�O�l�`���쐬
    CD3DX12_DESCRIPTOR_RANGE descriptorRanges[2];

    // UAV �����W�i�o�̓e�N�X�`���j
    descriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // u0

    // SRV �����W�i�}�e���A���A���_�A�C���f�b�N�X�A�I�t�Z�b�g�o�b�t�@�j
    descriptorRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1);  // t1, t2, t3, t4

    CD3DX12_ROOT_PARAMETER rootParameters[4];
    rootParameters[0].InitAsDescriptorTable(1, &descriptorRanges[0]);  // �o��UAV
    rootParameters[1].InitAsShaderResourceView(0);                     // TLAS (t0)
    rootParameters[2].InitAsConstantBufferView(0);                     // �V�[���萔�o�b�t�@ (b0)
    rootParameters[3].InitAsDescriptorTable(1, &descriptorRanges[1]);  // SRV�e�[�u�� (t1-t4)

    CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&globalRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

    if ( FAILED(hr) ) {
        if ( error ) {
            std::string errorMsg(static_cast<char*>( error->GetBufferPointer() ));
            throw std::runtime_error("Failed to serialize root signature: " + errorMsg);
        }
        throw std::runtime_error("Failed to serialize root signature");
    }

    hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_globalRootSignature));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create global root signature");
    }
}

void DXRRenderer::CreateRaytracingPipelineStateObject() {
    // �V�F�[�_�[�ǂݍ���
    auto rayGenShader = LoadOrCompileShader(L"Src/Shader/RayGen.hlsl", L"RayGen");
    auto missShader = LoadOrCompileShader(L"Src/Shader/Miss.hlsl", L"Miss");
    auto lambertianHitShader = LoadOrCompileShader(L"Src/Shader/ClosestHit_Lambertian.hlsl", L"ClosestHit_Lambertian");
    auto metalHitShader = LoadOrCompileShader(L"Src/Shader/ClosestHit_Metal.hlsl", L"ClosestHit_Metal");
    auto dielectricHitShader = LoadOrCompileShader(L"Src/Shader/ClosestHit_Dielectric.hlsl", L"ClosestHit_Dielectric");
    auto lightHitShader = LoadOrCompileShader(L"Src/Shader/ClosestHit_DiffuseLight.hlsl", L"ClosestHit_DiffuseLight");

    // �G�N�X�|�[�g�����Œ蕶����Ƃ��Ē�`
    static const wchar_t* exportNames[] = {
        L"RayGen", L"Miss",
        L"ClosestHit_Lambertian", L"ClosestHit_Metal",
        L"ClosestHit_Dielectric", L"ClosestHit_DiffuseLight"
    };

    static const wchar_t* hitGroupNames[] = {
        L"HitGroup_Lambertian", L"HitGroup_Metal",
        L"HitGroup_Dielectric", L"HitGroup_DiffuseLight"
    };

    static const wchar_t* closestHitNames[] = {
        L"ClosestHit_Lambertian", L"ClosestHit_Metal",
        L"ClosestHit_Dielectric", L"ClosestHit_DiffuseLight"
    };

    std::vector<ComPtr<IDxcBlob>> shaders = {
        rayGenShader, missShader, lambertianHitShader,
        metalHitShader, dielectricHitShader, lightHitShader
    };

    // �Œ�T�C�Y�̔z����g�p
    D3D12_EXPORT_DESC exportDescs[6];
    D3D12_DXIL_LIBRARY_DESC dxilLibDescs[6];
    D3D12_HIT_GROUP_DESC hitGroupDescs[4];
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfigDesc = {};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderConfigAssociationDesc = {};

    // ������ ���[�J�����[�g�V�O�l�`���֘A��ǉ� ������
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION localRootSigAssociationDesc = {};

    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.reserve(16); // �]�T���������T�C�Y

    // DXIL���C�u�����T�u�I�u�W�F�N�g�쐬
    for ( int i = 0; i < 6; ++i ) {
        exportDescs[i].Name = exportNames[i];
        exportDescs[i].ExportToRename = nullptr;
        exportDescs[i].Flags = D3D12_EXPORT_FLAG_NONE;

        dxilLibDescs[i].DXILLibrary.BytecodeLength = shaders[i]->GetBufferSize();
        dxilLibDescs[i].DXILLibrary.pShaderBytecode = shaders[i]->GetBufferPointer();
        dxilLibDescs[i].NumExports = 1;
        dxilLibDescs[i].pExports = &exportDescs[i];

        D3D12_STATE_SUBOBJECT subobj = {};
        subobj.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        subobj.pDesc = &dxilLibDescs[i];
        subobjects.push_back(subobj);
    }

    // �q�b�g�O���[�v�T�u�I�u�W�F�N�g�쐬
    for ( int i = 0; i < 4; ++i ) {
        hitGroupDescs[i].HitGroupExport = hitGroupNames[i];
        hitGroupDescs[i].Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hitGroupDescs[i].ClosestHitShaderImport = closestHitNames[i];
        hitGroupDescs[i].AnyHitShaderImport = nullptr;
        hitGroupDescs[i].IntersectionShaderImport = nullptr;

        D3D12_STATE_SUBOBJECT subobj = {};
        subobj.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        subobj.pDesc = &hitGroupDescs[i];
        subobjects.push_back(subobj);
    }

    // �V�F�[�_�[�ݒ�T�u�I�u�W�F�N�g
    shaderConfigDesc.MaxPayloadSizeInBytes = ( 3 * sizeof(float) ) + ( 2 * sizeof(unsigned int) );
    shaderConfigDesc.MaxAttributeSizeInBytes = sizeof(float) * 2;

    D3D12_STATE_SUBOBJECT shaderConfigSubobject = {};
    shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigSubobject.pDesc = &shaderConfigDesc;
    subobjects.push_back(shaderConfigSubobject);

    // �V�F�[�_�[�ݒ�����ׂẴV�F�[�_�[�G�N�X�|�[�g�Ɋ֘A�t��
    shaderConfigAssociationDesc.pSubobjectToAssociate = &subobjects.back();
    shaderConfigAssociationDesc.NumExports = _countof(exportNames);
    shaderConfigAssociationDesc.pExports = exportNames;

    D3D12_STATE_SUBOBJECT shaderConfigAssociationSubobject = {};
    shaderConfigAssociationSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    shaderConfigAssociationSubobject.pDesc = &shaderConfigAssociationDesc;
    subobjects.push_back(shaderConfigAssociationSubobject);

    // �O���[�o�����[�g�V�O�l�`��
    D3D12_STATE_SUBOBJECT globalRootSigSubobject = {};
    globalRootSigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    globalRootSigSubobject.pDesc = m_globalRootSignature.GetAddressOf();
    subobjects.push_back(globalRootSigSubobject);

    // �p�C�v���C���ݒ�
    static D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = { 8 };

    D3D12_STATE_SUBOBJECT pipelineConfigSubobject = {};
    pipelineConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigSubobject.pDesc = &pipelineConfig;
    subobjects.push_back(pipelineConfigSubobject);

    // �X�e�[�g�I�u�W�F�N�g�쐬
    D3D12_STATE_OBJECT_DESC raytracingPipelineDesc = {};
    raytracingPipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    raytracingPipelineDesc.NumSubobjects = static_cast<UINT>( subobjects.size() );
    raytracingPipelineDesc.pSubobjects = subobjects.data();

    HRESULT hr = m_device->CreateStateObject(&raytracingPipelineDesc, IID_PPV_ARGS(&m_rtStateObject));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create raytracing pipeline state object");
    }
}

ComPtr<IDxcBlob> DXRRenderer::LoadCSO(const std::wstring& filename) {
    // �t�@�C�����J��
    std::ifstream file(filename, std::ios::binary);
    if ( !file.is_open() ) {
        std::string errorMsg = "Failed to open shader file: ";
        std::string filenameStr(filename.begin(), filename.end());
        throw std::runtime_error(errorMsg + filenameStr);
    }

    // �t�@�C���T�C�Y�擾
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // �f�[�^�ǂݍ���
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>( data.data() ), fileSize);
    file.close();

    if ( !m_dxcUtils ) {
        // ���� m_dxcUtils �����̎��_�ŏ���������Ă��Ȃ��ꍇ�̃t�H�[���o�b�N����
        // (�{���� DXRRenderer �̏��������ɍs���ׂ�)
        HRESULT hr_init = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_dxcUtils));
        if ( FAILED(hr_init) ) {
            throw std::runtime_error("Failed to create DxcUtils instance in LoadCSO");
        }
    }

    ComPtr<IDxcBlobEncoding> blobEncoding; // CreateBlob �� IDxcBlobEncoding** �����҂��邽�߁A���̌^�Ŏ󂯂�
    HRESULT hr = m_dxcUtils->CreateBlob(
        data.data(),
        static_cast<UINT32>( data.size() ),
        DXC_CP_UTF8,
        blobEncoding.GetAddressOf() // ������ ComPtr �̎g����: IDxcBlobEncoding** ��n��
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create blob from shader data using IDxcUtils::CreateBlob");
    }

    ComPtr<IDxcBlob> resultBlob;
    // IDxcBlobEncoding ���� IDxcBlob �C���^�[�t�F�[�X���擾 (QueryInterface �ɑ���)
    // IDxcBlob �� IDxcBlobEncoding ���p�����Ă��邽�߁A���̕ϊ��͐�������͂��ł��B
    hr = blobEncoding.As(&resultBlob);
    if ( FAILED(hr) ) {
        // ���̃G���[�͒ʏ픭�����ɂ����ł����A�O�̂��߃`�F�b�N
        throw std::runtime_error("Failed to obtain IDxcBlob interface from blob encoding");
    }

    return resultBlob;
}

UINT DXRRenderer::AlignTo(UINT size, UINT alignment) {
    return ( size + alignment - 1 ) & ~( alignment - 1 );
}

void DXRRenderer::UpdateCamera() {
    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );

    if ( !dxrScene ) return;

    auto cameraData = dxrScene->GetCamera();

    // �r���[�E�v���W�F�N�V�����s��v�Z
    XMVECTOR eyePos = XMLoadFloat3(&cameraData.position);
    XMVECTOR targetPos = XMLoadFloat3(&cameraData.target);
    XMVECTOR upVec = XMLoadFloat3(&cameraData.up);

    XMMATRIX viewMatrix = XMMatrixInverse(nullptr, XMMatrixLookAtLH(eyePos, targetPos, upVec));
    XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(cameraData.fov, cameraData.aspect, 0.1f, 1000.0f);

    // �J��������I�u�W�F�N�g�܂ł̋����v�Z
    float distToSphere1 = sqrtf(powf(190.0f - cameraData.position.x, 2) +
        powf(90.0f - cameraData.position.y, 2) +
        powf(190.0f - cameraData.position.z, 2));

    SceneConstantBuffer sceneConstants;
    sceneConstants.projectionMatrix = projMatrix;
    sceneConstants.viewMatrix = viewMatrix;
    sceneConstants.lightPosition = XMFLOAT4(0.0f, 554.0f, 279.5f, 1.0f);
    sceneConstants.lightColor = XMFLOAT4(15.0f, 15.0f, 15.0f, 1.0f);

    // �萔�o�b�t�@�X�V
    void* mappedData;
    m_sceneConstantBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, &sceneConstants, sizeof(SceneConstantBuffer));
    m_sceneConstantBuffer->Unmap(0, nullptr);

    OutputDebugStringA("Camera update completed\n");
}

// DXRRenderer_AccelerationStructure.cpp
// DXRRenderer.cpp�ɒǉ����镔��

void DXRRenderer::CreateAccelerationStructures() {
    // ���݂̃V�[������TLAS�f�[�^�擾
    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );

    if ( !dxrScene ) {
        OutputDebugStringA("ERROR: No DXRScene found!\n");
        return;
    }

    TLASData tlasData = dxrScene->GetTLASData();

    // �ڍׂȃf�o�b�O���
    char debugMsg[512];
    sprintf_s(debugMsg, "=== Acceleration Structure Detailed Debug ===\n");
    OutputDebugStringA(debugMsg);
    sprintf_s(debugMsg, "Total BLAS count: %zu\n", tlasData.blasDataList.size());
    OutputDebugStringA(debugMsg);

    if ( tlasData.blasDataList.empty() ) {
        OutputDebugStringA("ERROR: No BLAS data found! No objects to render.\n");
        return;
    }

    // ������BLAS/TLAS�쐬����...
    m_bottomLevelAS.clear();
    m_bottomLevelASScratch.clear();

    Singleton<Renderer>::getInstance().ResetCommandList();

    for ( size_t i = 0; i < tlasData.blasDataList.size(); ++i ) {
        sprintf_s(debugMsg, "Creating BLAS[%zu]...\n", i);
        OutputDebugStringA(debugMsg);

        ComPtr<ID3D12Resource> blasBuffer;
        CreateBLAS(tlasData.blasDataList[i], blasBuffer);
        m_bottomLevelAS.push_back(blasBuffer);
    }

    // BLAS�o���A
    std::vector<CD3DX12_RESOURCE_BARRIER> blasBarriers;
    for ( auto& blas : m_bottomLevelAS ) {
        blasBarriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(blas.Get()));
    }
    if ( !blasBarriers.empty() ) {
        m_commandList->ResourceBarrier(static_cast<UINT>( blasBarriers.size() ), blasBarriers.data());
    }

    // TLAS�쐬�i�C���X�^���X���ߍ��ݔŁj
    OutputDebugStringA("Creating TLAS with embedded materials...\n");
    CreateTLAS(tlasData);

    // ������ �V�K�ǉ��F�f�B�X�N���v�^�쐬 ������
    CreateDescriptorsForBuffers(tlasData);

    Singleton<Renderer>::getInstance().ExecuteCommandListAndWait();
    OutputDebugStringA("Acceleration structures created successfully\n");
}

void DXRRenderer::CreateBLAS(BLASData& blasData, ComPtr<ID3D12Resource>& blasBuffer) {
    HRESULT hr;

    char debugMsg[512];
    sprintf_s(debugMsg, "=== Creating BLAS (FIXED) ===\n");
    OutputDebugStringA(debugMsg);

    // ������ ���̓f�[�^�̏ڍ׌��� ������
    if ( blasData.vertices.empty() || blasData.indices.empty() ) {
        OutputDebugStringA("ERROR: Empty vertices or indices in BLAS!\n");
        return;
    }

    sprintf_s(debugMsg, "Input data: %zu vertices, %zu indices\n",
        blasData.vertices.size(), blasData.indices.size());
    OutputDebugStringA(debugMsg);

    // ������ �C���f�b�N�X�̑Ó��������S���� ������
    uint32_t maxValidIndex = static_cast<uint32_t>( blasData.vertices.size() - 1 );
    bool hasInvalidIndices = false;

    for ( size_t i = 0; i < blasData.indices.size(); ++i ) {
        uint32_t idx = blasData.indices[i];
        if ( idx > maxValidIndex ) {
            sprintf_s(debugMsg, "ERROR: Invalid index %u at position %zu (max: %u)\n",
                idx, i, maxValidIndex);
            OutputDebugStringA(debugMsg);
            hasInvalidIndices = true;
        }
    }

    if ( hasInvalidIndices ) {
        OutputDebugStringA("ABORTING BLAS creation due to invalid indices!\n");
        return;
    }

    // ������ �f�[�^�T���v���̏ڍ׏o�� ������
    sprintf_s(debugMsg, "First 5 vertices:\n");
    OutputDebugStringA(debugMsg);
    for ( int i = 0; i < min(5, (int)blasData.vertices.size()); ++i ) {
        sprintf_s(debugMsg, "  [%d]: pos=(%.3f,%.3f,%.3f), normal=(%.3f,%.3f,%.3f)\n", i,
            blasData.vertices[i].position.x, blasData.vertices[i].position.y, blasData.vertices[i].position.z,
            blasData.vertices[i].normal.x, blasData.vertices[i].normal.y, blasData.vertices[i].normal.z);
        OutputDebugStringA(debugMsg);
    }

    sprintf_s(debugMsg, "First 9 indices: ");
    OutputDebugStringA(debugMsg);
    for ( int i = 0; i < min(9, (int)blasData.indices.size()); ++i ) {
        sprintf_s(debugMsg, "%u ", blasData.indices[i]);
        OutputDebugStringA(debugMsg);
    }
    sprintf_s(debugMsg, "\n");
    OutputDebugStringA(debugMsg);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    // ������ ��p�̒��_�o�b�t�@���ʍ쐬 ������
    UINT vertexBufferSize = static_cast<UINT>( blasData.vertices.size() * sizeof(DXRVertex) );
    sprintf_s(debugMsg, "Creating dedicated vertex buffer: %u bytes\n", vertexBufferSize);
    OutputDebugStringA(debugMsg);

    CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&blasData.vertexBuffer)
    );

    if ( FAILED(hr) ) {
        sprintf_s(debugMsg, "ERROR: Failed to create vertex buffer (HRESULT: 0x%08X)\n", hr);
        OutputDebugStringA(debugMsg);
        throw std::runtime_error("Failed to create vertex buffer for BLAS");
    }

    // ������ ���_�f�[�^���A�b�v���[�h�i���ؕt���j������
    void* mappedVertexData;
    hr = blasData.vertexBuffer->Map(0, nullptr, &mappedVertexData);
    if ( FAILED(hr) ) {
        OutputDebugStringA("ERROR: Failed to map vertex buffer\n");
        throw std::runtime_error("Failed to map vertex buffer");
    }

    memcpy(mappedVertexData, blasData.vertices.data(), vertexBufferSize);
    blasData.vertexBuffer->Unmap(0, nullptr);

    OutputDebugStringA("Vertex buffer uploaded successfully\n");

    // ������ ��p�̃C���f�b�N�X�o�b�t�@���ʍ쐬 ������
    UINT indexBufferSize = static_cast<UINT>( blasData.indices.size() * sizeof(uint32_t) );
    sprintf_s(debugMsg, "Creating dedicated index buffer: %u bytes\n", indexBufferSize);
    OutputDebugStringA(debugMsg);

    CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&blasData.indexBuffer)
    );

    if ( FAILED(hr) ) {
        sprintf_s(debugMsg, "ERROR: Failed to create index buffer (HRESULT: 0x%08X)\n", hr);
        OutputDebugStringA(debugMsg);
        throw std::runtime_error("Failed to create index buffer for BLAS");
    }

    // ������ �C���f�b�N�X�f�[�^���A�b�v���[�h�i���ؕt���j������
    void* mappedIndexData;
    hr = blasData.indexBuffer->Map(0, nullptr, &mappedIndexData);
    if ( FAILED(hr) ) {
        OutputDebugStringA("ERROR: Failed to map index buffer\n");
        throw std::runtime_error("Failed to map index buffer");
    }

    memcpy(mappedIndexData, blasData.indices.data(), indexBufferSize);
    blasData.indexBuffer->Unmap(0, nullptr);

    OutputDebugStringA("Index buffer uploaded successfully\n");

    // ������ �W�I���g���f�B�X�N���v�^�ݒ�i�ڍ׃��O�t���j������
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    // ���_�f�[�^�ݒ�
    geometryDesc.Triangles.VertexBuffer.StartAddress = blasData.vertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(DXRVertex);
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount = static_cast<UINT>( blasData.vertices.size() );

    // �C���f�b�N�X�f�[�^�ݒ�
    geometryDesc.Triangles.IndexBuffer = blasData.indexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geometryDesc.Triangles.IndexCount = static_cast<UINT>( blasData.indices.size() );

    sprintf_s(debugMsg, "Geometry setup: VertexCount=%u, IndexCount=%u\n",
        geometryDesc.Triangles.VertexCount, geometryDesc.Triangles.IndexCount);
    sprintf_s(debugMsg, "Expected triangles: %u\n", geometryDesc.Triangles.IndexCount / 3);
    OutputDebugStringA(debugMsg);

    // BLAS"���͐ݒ�
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
    blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    blasInputs.NumDescs = 1;
    blasInputs.pGeometryDescs = &geometryDesc;

    // �ȉ��A������BLAS�\�z����...
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuildInfo);

    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC blasBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
        blasPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &blasBufferDesc,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nullptr,
        IID_PPV_ARGS(&blasBuffer)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create BLAS buffer");
    }

    CD3DX12_RESOURCE_DESC scratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
        blasPrebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &scratchBufferDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&blasData.scratchBuffer)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create BLAS scratch buffer");
    }

    m_bottomLevelASScratch.push_back(blasData.scratchBuffer);

    // BLAS�\�z���s
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
    blasDesc.Inputs = blasInputs;
    blasDesc.DestAccelerationStructureData = blasBuffer->GetGPUVirtualAddress();
    blasDesc.ScratchAccelerationStructureData = blasData.scratchBuffer->GetGPUVirtualAddress();

    m_commandList->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);

    sprintf_s(debugMsg, "BLAS construction completed successfully\n");
    OutputDebugStringA(debugMsg);
}

void DXRRenderer::CreateTLAS(TLASData& tlasData) {
    char debugMsg[512];

    sprintf_s(debugMsg, "=== CreateTLAS with Transform Verification ===\n");
    OutputDebugStringA(debugMsg);

    // �C���X�^���X�f�B�X�N���v�^�쐬
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

    for ( size_t i = 0; i < tlasData.blasDataList.size(); ++i ) {
        const auto& blasData = tlasData.blasDataList[i];

        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};

        // ������ �d�v�F�ϊ��s��̓K�؂ȓK�p ������
        XMMATRIX transform = blasData.transform;

        // �f�o�b�O�F�ϊ��s��̓��e���m�F
        XMFLOAT4X4 transformFloat;
        XMStoreFloat4x4(&transformFloat, transform);
        sprintf_s(debugMsg, "Instance[%zu] Transform Matrix:\n", i);
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "  [%.3f, %.3f, %.3f, %.3f]\n",
            transformFloat._11, transformFloat._12, transformFloat._13, transformFloat._14);
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "  [%.3f, %.3f, %.3f, %.3f]\n",
            transformFloat._21, transformFloat._22, transformFloat._23, transformFloat._24);
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "  [%.3f, %.3f, %.3f, %.3f]\n",
            transformFloat._31, transformFloat._32, transformFloat._33, transformFloat._34);
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "  [%.3f, %.3f, %.3f, %.3f]\n",
            transformFloat._41, transformFloat._42, transformFloat._43, transformFloat._44);
        OutputDebugStringA(debugMsg);

        // XMFLOAT3X4�ɕϊ��i4�s�ڂ͕s�v�j
        XMFLOAT3X4 transform3x4;
        XMStoreFloat3x4(&transform3x4, transform);

        // �C���X�^���X�ϊ��s���ݒ�
        for ( int row = 0; row < 3; ++row ) {
            for ( int col = 0; col < 4; ++col ) {
                instanceDesc.Transform[row][col] = transform3x4.m[row][col];
            }
        }

        // ������ �C���X�^���XID�ƃ}�e���A���^�C�v�̐ݒ� ������
        instanceDesc.InstanceID = static_cast<UINT>( i );
        instanceDesc.InstanceMask = 0xFF;

        // �}�e���A���^�C�v�Ńq�b�g�O���[�v��I��
        instanceDesc.InstanceContributionToHitGroupIndex = static_cast<UINT>( blasData.material.materialType );
        instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc.AccelerationStructure = m_bottomLevelAS[i]->GetGPUVirtualAddress();

        sprintf_s(debugMsg, "Instance[%zu]: ID=%u, MaterialType=%d, Albedo=(%.2f,%.2f,%.2f)\n",
            i, instanceDesc.InstanceID, blasData.material.materialType,
            blasData.material.albedo.x, blasData.material.albedo.y, blasData.material.albedo.z);
        OutputDebugStringA(debugMsg);

        instanceDescs.push_back(instanceDesc);
    }

    // ������ �}�e���A���o�b�t�@�쐬 ������
    CreateMaterialBuffer(tlasData);

    // ������ ���_�E�C���f�b�N�X�o�b�t�@�쐬 ������
    CreateVertexIndexBuffers(tlasData);

    // ������TLAS�쐬����...
    UINT instanceBufferSize = static_cast<UINT>( instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC) );

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC instanceBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(instanceBufferSize);

    HRESULT hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &instanceBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&tlasData.instanceBuffer)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create instance buffer");
    }

    void* mappedInstanceData;
    tlasData.instanceBuffer->Map(0, nullptr, &mappedInstanceData);
    memcpy(mappedInstanceData, instanceDescs.data(), instanceBufferSize);
    tlasData.instanceBuffer->Unmap(0, nullptr);

    // TLAS�r���h����
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    tlasInputs.NumDescs = static_cast<UINT>( instanceDescs.size() );
    tlasInputs.InstanceDescs = tlasData.instanceBuffer->GetGPUVirtualAddress();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC tlasBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
        tlasPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &tlasBufferDesc,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nullptr,
        IID_PPV_ARGS(&m_topLevelAS)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create TLAS buffer");
    }

    CD3DX12_RESOURCE_DESC tlasScratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
        tlasPrebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &tlasScratchBufferDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_topLevelASScratch)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create TLAS scratch buffer");
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {};
    tlasDesc.Inputs = tlasInputs;
    tlasDesc.DestAccelerationStructureData = m_topLevelAS->GetGPUVirtualAddress();
    tlasDesc.ScratchAccelerationStructureData = m_topLevelASScratch->GetGPUVirtualAddress();

    m_commandList->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_topLevelAS.Get());
    m_commandList->ResourceBarrier(1, &barrier);

    OutputDebugStringA("TLAS created with verified transforms\n");
}

void DXRRenderer::CreateDescriptorsForBuffers(const TLASData& tlasData) {
    UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // UAV�i�o�̓e�N�X�`���j���X�L�b�v
    cpuHandle.Offset(1, descriptorSize);

    // ������ �}�e���A���o�b�t�@��SRV�쐬 (t1) ������
    if ( m_materialBuffer ) {
        D3D12_SHADER_RESOURCE_VIEW_DESC materialSrvDesc = {};
        materialSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        materialSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        materialSrvDesc.Buffer.FirstElement = 0;
        materialSrvDesc.Buffer.NumElements = static_cast<UINT>( tlasData.blasDataList.size() );
        materialSrvDesc.Buffer.StructureByteStride = sizeof(DXRMaterialData);
        materialSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        m_device->CreateShaderResourceView(m_materialBuffer.Get(), &materialSrvDesc, cpuHandle);

        char debugMsg[256];
        sprintf_s(debugMsg, "Material buffer SRV created at t1: %zu materials\n", tlasData.blasDataList.size());
        OutputDebugStringA(debugMsg);
    }
    cpuHandle.Offset(1, descriptorSize);

    // ������ ���_�o�b�t�@��SRV�쐬 (t2) ������
    if ( m_globalVertexBuffer ) {
        D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc = {};
        vertexSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        vertexSrvDesc.Buffer.FirstElement = 0;
        vertexSrvDesc.Buffer.NumElements = m_totalVertexCount;
        vertexSrvDesc.Buffer.StructureByteStride = sizeof(DXRVertex);
        vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        m_device->CreateShaderResourceView(m_globalVertexBuffer.Get(), &vertexSrvDesc, cpuHandle);
        OutputDebugStringA("Vertex buffer SRV created at t2\n");
    }
    cpuHandle.Offset(1, descriptorSize);

    // ������ �C���f�b�N�X�o�b�t�@��SRV�쐬 (t3) ������
    if ( m_globalIndexBuffer ) {
        D3D12_SHADER_RESOURCE_VIEW_DESC indexSrvDesc = {};
        indexSrvDesc.Format = DXGI_FORMAT_R32_UINT;
        indexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        indexSrvDesc.Buffer.FirstElement = 0;
        indexSrvDesc.Buffer.NumElements = m_totalIndexCount;
        indexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        m_device->CreateShaderResourceView(m_globalIndexBuffer.Get(), &indexSrvDesc, cpuHandle);
        OutputDebugStringA("Index buffer SRV created at t3\n");
    }
    cpuHandle.Offset(1, descriptorSize);

    // ������ �C���X�^���X�I�t�Z�b�g�o�b�t�@��SRV�쐬 (t4) ������
    if ( m_instanceOffsetBuffer ) {
        D3D12_SHADER_RESOURCE_VIEW_DESC offsetSrvDesc = {};
        offsetSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        offsetSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        offsetSrvDesc.Buffer.FirstElement = 0;
        offsetSrvDesc.Buffer.NumElements = static_cast<UINT>( tlasData.blasDataList.size() );
        offsetSrvDesc.Buffer.StructureByteStride = 16; // InstanceOffsetData �̃T�C�Y
        offsetSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        m_device->CreateShaderResourceView(m_instanceOffsetBuffer.Get(), &offsetSrvDesc, cpuHandle);
        OutputDebugStringA("Instance offset buffer SRV created at t4\n");
    }
}

void DXRRenderer::CreateVertexIndexBuffers(const TLASData& tlasData) {
    char debugMsg[256];
    sprintf_s(debugMsg, "=== Creating FIXED Unified Global Buffers (SPHERE-SAFE) ===\n");
    OutputDebugStringA(debugMsg);

    // ������ �S�̂̃T�C�Y���v�Z ������
    m_totalVertexCount = 0;
    m_totalIndexCount = 0;

    for ( const auto& blasData : tlasData.blasDataList ) {
        m_totalVertexCount += static_cast<UINT>( blasData.vertices.size() );
        m_totalIndexCount += static_cast<UINT>( blasData.indices.size() );
    }

    sprintf_s(debugMsg, "Total vertices: %u, Total indices: %u\n", m_totalVertexCount, m_totalIndexCount);
    OutputDebugStringA(debugMsg);

    if ( m_totalVertexCount == 0 || m_totalIndexCount == 0 ) {
        OutputDebugStringA("ERROR: No vertex/index data to create unified buffers!\n");
        return;
    }

    // ������ �������_�E�C���f�b�N�X�f�[�^�\�z�i�C���Łj ������
    std::vector<DXRVertex> allVertices;
    std::vector<uint32_t> allIndices;
    std::vector<uint32_t> vertexOffsets;
    std::vector<uint32_t> indexOffsets;

    allVertices.reserve(m_totalVertexCount);
    allIndices.reserve(m_totalIndexCount);

    uint32_t currentVertexOffset = 0;
    uint32_t currentIndexOffset = 0;

    for ( size_t i = 0; i < tlasData.blasDataList.size(); ++i ) {
        const auto& blasData = tlasData.blasDataList[i];

        // ������ �I�t�Z�b�g���L�^�i�C���f�b�N�X�z����ł̃I�t�Z�b�g�j ������
        vertexOffsets.push_back(currentVertexOffset);
        indexOffsets.push_back(currentIndexOffset);

        sprintf_s(debugMsg, "Instance[%zu]: %zu vertices, %zu indices\n",
            i, blasData.vertices.size(), blasData.indices.size());
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "  -> Will be placed at vertexOffset=%u, indexOffset=%u\n",
            currentVertexOffset, currentIndexOffset);
        OutputDebugStringA(debugMsg);

        // ������ ���_�f�[�^�����̂܂܃R�s�[ ������
        allVertices.insert(allVertices.end(), blasData.vertices.begin(), blasData.vertices.end());

        // ������ �d�v�F�C���f�b�N�X�f�[�^�͌��̒l�̂܂܁A���_�I�t�Z�b�g�͕ʊǗ� ������
        // ����ɂ��e�I�u�W�F�N�g�̌��̒��_�\�����ێ������
        allIndices.insert(allIndices.end(), blasData.indices.begin(), blasData.indices.end());

        // �f�o�b�O�F���̃C���f�b�N�X�l���m�F
        sprintf_s(debugMsg, "  Original indices (first 6): ");
        OutputDebugStringA(debugMsg);
        for ( size_t j = 0; j < min(6, blasData.indices.size()); ++j ) {
            sprintf_s(debugMsg, "%u ", blasData.indices[j]);
            OutputDebugStringA(debugMsg);
        }
        sprintf_s(debugMsg, "\n");
        OutputDebugStringA(debugMsg);

        // ���̃I�t�Z�b�g���X�V
        currentVertexOffset += static_cast<uint32_t>( blasData.vertices.size() );
        currentIndexOffset += static_cast<uint32_t>( blasData.indices.size() );
    }

    // ������ �������_�o�b�t�@���쐬 ������
    {
        UINT vertexBufferSize = static_cast<UINT>( allVertices.size() * sizeof(DXRVertex) );
        sprintf_s(debugMsg, "Creating unified vertex buffer: %u bytes (%zu vertices)\n",
            vertexBufferSize, allVertices.size());
        OutputDebugStringA(debugMsg);

        CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

        HRESULT hr = m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &vertexBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_globalVertexBuffer)
        );

        if ( SUCCEEDED(hr) ) {
            void* mappedData;
            m_globalVertexBuffer->Map(0, nullptr, &mappedData);
            memcpy(mappedData, allVertices.data(), vertexBufferSize);
            m_globalVertexBuffer->Unmap(0, nullptr);
            OutputDebugStringA("Unified vertex buffer created successfully\n");
        }
        else {
            sprintf_s(debugMsg, "ERROR: Failed to create unified vertex buffer (HRESULT: 0x%08X)\n", hr);
            OutputDebugStringA(debugMsg);
            return;
        }
    }

    // ������ �����C���f�b�N�X�o�b�t�@���쐬 ������
    {
        UINT indexBufferSize = static_cast<UINT>( allIndices.size() * sizeof(uint32_t) );
        sprintf_s(debugMsg, "Creating unified index buffer: %u bytes (%zu indices)\n",
            indexBufferSize, allIndices.size());
        OutputDebugStringA(debugMsg);

        CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

        HRESULT hr = m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &indexBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_globalIndexBuffer)
        );

        if ( SUCCEEDED(hr) ) {
            void* mappedData;
            m_globalIndexBuffer->Map(0, nullptr, &mappedData);
            memcpy(mappedData, allIndices.data(), indexBufferSize);
            m_globalIndexBuffer->Unmap(0, nullptr);
            OutputDebugStringA("Unified index buffer created successfully\n");
        }
        else {
            sprintf_s(debugMsg, "ERROR: Failed to create unified index buffer (HRESULT: 0x%08X)\n", hr);
            OutputDebugStringA(debugMsg);
            return;
        }
    }

    // ������ �I�t�Z�b�g���o�b�t�@���쐬 ������
    CreateInstanceOffsetBuffer(vertexOffsets, indexOffsets);

    // ������ ���؁F�쐬���ꂽ�f�[�^�̐��������`�F�b�N ������
    sprintf_s(debugMsg, "=== Unified Buffer Validation (SPHERE-SAFE) ===\n");
    OutputDebugStringA(debugMsg);

    for ( size_t i = 0; i < min(3, tlasData.blasDataList.size()); ++i ) {
        uint32_t vOffset = vertexOffsets[i];
        uint32_t iOffset = indexOffsets[i];

        sprintf_s(debugMsg, "Instance[%zu]: vOffset=%u, iOffset=%u\n", i, vOffset, iOffset);
        OutputDebugStringA(debugMsg);

        // �ŏ��̎O�p�`�̒��_���m�F
        if ( iOffset + 2 < allIndices.size() ) {
            uint32_t i0 = allIndices[iOffset + 0] + vOffset;  // �I�t�Z�b�g�K�p
            uint32_t i1 = allIndices[iOffset + 1] + vOffset;
            uint32_t i2 = allIndices[iOffset + 2] + vOffset;

            sprintf_s(debugMsg, "  Triangle 0 indices: %u, %u, %u\n", i0, i1, i2);
            OutputDebugStringA(debugMsg);

            if ( i0 < allVertices.size() && i1 < allVertices.size() && i2 < allVertices.size() ) {
                sprintf_s(debugMsg, "  Vertex[%u]: pos=(%.3f,%.3f,%.3f)\n", i0,
                    allVertices[i0].position.x, allVertices[i0].position.y, allVertices[i0].position.z);
                OutputDebugStringA(debugMsg);
            }
        }
    }

    OutputDebugStringA("SPHERE-SAFE unified buffer creation completed successfully!\n");
}

void DXRRenderer::CreateInstanceOffsetBuffer(const std::vector<uint32_t>& vertexOffsets, const std::vector<uint32_t>& indexOffsets) {
    struct InstanceOffsetData {
        uint32_t vertexOffset;
        uint32_t indexOffset;
        uint32_t padding[2]; // 16�o�C�g�A���C�����g
    };

    std::vector<InstanceOffsetData> offsetArray;

    char debugMsg[256];
    sprintf_s(debugMsg, "=== Creating Instance Offset Buffer (REAL OFFSETS) ===\n");
    OutputDebugStringA(debugMsg);

    // ������ �T�C�Y�`�F�b�N ������
    if ( vertexOffsets.size() != indexOffsets.size() ) {
        sprintf_s(debugMsg, "ERROR: Offset array size mismatch! vertex=%zu, index=%zu\n",
            vertexOffsets.size(), indexOffsets.size());
        OutputDebugStringA(debugMsg);
        return;
    }

    for ( size_t i = 0; i < vertexOffsets.size(); ++i ) {
        InstanceOffsetData offset;
        offset.vertexOffset = vertexOffsets[i];
        offset.indexOffset = indexOffsets[i];
        offset.padding[0] = 0;
        offset.padding[1] = 0;
        offsetArray.push_back(offset);

        sprintf_s(debugMsg, "Instance[%zu]: vertexOffset=%u, indexOffset=%u\n",
            i, offset.vertexOffset, offset.indexOffset);
        OutputDebugStringA(debugMsg);
    }


    UINT offsetBufferSize = static_cast<UINT>( offsetArray.size() * sizeof(InstanceOffsetData) );
    sprintf_s(debugMsg, "Offset buffer size: %u bytes (%zu entries)\n",
        offsetBufferSize, offsetArray.size());
    OutputDebugStringA(debugMsg);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC offsetBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(offsetBufferSize);

    HRESULT hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &offsetBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_instanceOffsetBuffer)
    );

    if ( SUCCEEDED(hr) ) {
        void* mappedData;
        m_instanceOffsetBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, offsetArray.data(), offsetBufferSize);
        m_instanceOffsetBuffer->Unmap(0, nullptr);

        sprintf_s(debugMsg, "Instance offset buffer created successfully\n");
        OutputDebugStringA(debugMsg);
    }
    else {
        sprintf_s(debugMsg, "ERROR: Failed to create instance offset buffer (HRESULT: 0x%08X)\n", hr);
        OutputDebugStringA(debugMsg);
    }
}

ComPtr<IDxcBlob> DXRRenderer::CompileShaderFromFile(const std::wstring& hlslPath, const std::wstring& entryPoint, const std::wstring& target) {
    // HLSL�t�@�C����ǂݍ���
    ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = m_dxcUtils->LoadFile(hlslPath.c_str(), nullptr, &sourceBlob);
    if ( FAILED(hr) ) {
        std::string errorMsg = "Failed to load HLSL file: ";
        std::string filenameStr(hlslPath.begin(), hlslPath.end());
        throw std::runtime_error(errorMsg + filenameStr);
    }

    // �R���p�C�������̐ݒ�
    std::vector<LPCWSTR> arguments = {
        hlslPath.c_str(),               // �t�@�C�����i�f�o�b�O�p�j
        L"-E", entryPoint.c_str(),      // �G���g���[�|�C���g
        L"-T", target.c_str(),          // �^�[�Q�b�g�ilib_6_5�Ȃǁj
        L"-O3",                         // �œK�����x��
        L"-Qstrip_debug",               // �f�o�b�O�����폜
        L"-Qstrip_reflect",             // ���t���N�V���������폜
    };

    // DXCBuffer���쐬
    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = CP_UTF8;

    // �R���p�C�����s
    ComPtr<IDxcResult> compileResult;
    hr = m_dxcCompiler->Compile(
        &sourceBuffer,
        arguments.data(),
        static_cast<UINT32>( arguments.size() ),
        m_includeHandler.Get(),
        IID_PPV_ARGS(&compileResult)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("DXC compilation failed");
    }

    // �R���p�C�����ʂ̊m�F
    HRESULT compileHR;
    compileResult->GetStatus(&compileHR);

    if ( FAILED(compileHR) ) {
        // �G���[���b�Z�[�W���擾
        ComPtr<IDxcBlobEncoding> errorBlob;
        if ( SUCCEEDED(compileResult->GetErrorBuffer(&errorBlob)) && errorBlob ) {
            std::string errorMsg = "Shader compilation failed:\n";
            errorMsg += static_cast<const char*>( errorBlob->GetBufferPointer() );
            throw std::runtime_error(errorMsg);
        }
        else {
            throw std::runtime_error("Shader compilation failed with unknown error");
        }
    }

    // �R���p�C���ς݃V�F�[�_�[���擾
    ComPtr<IDxcBlob> shaderBlob;
    hr = compileResult->GetResult(&shaderBlob);
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to get shader compilation result");
    }

    return shaderBlob;
}

ComPtr<IDxcBlob> DXRRenderer::LoadOrCompileShader(const std::wstring& hlslPath, const std::wstring& entryPoint, const std::wstring& target) {
    std::wstring csoPath = hlslPath;  
    size_t lastSlash = csoPath.find_last_of(L'/');  
    if (lastSlash != std::wstring::npos) {  
       csoPath = csoPath.substr(lastSlash + 1);  
    }  
    size_t lastDot = csoPath.find_last_of(L'.');  
    if (lastDot != std::wstring::npos) {  
       csoPath = csoPath.substr(0, lastDot) + L".cso";  
    } else {  
       csoPath += L".cso";  
    }  
    csoPath = L"Shader/" + csoPath;

    // DXC�R���p�C���[�̏������i��x�����j
    if ( !m_dxcUtils ) {
        HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_dxcUtils));
        if ( FAILED(hr) ) {
            throw std::runtime_error("Failed to create DxcUtils");
        }

        hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_dxcCompiler));
        if ( FAILED(hr) ) {
            throw std::runtime_error("Failed to create DxcCompiler");
        }

        hr = m_dxcUtils->CreateDefaultIncludeHandler(&m_includeHandler);
        if ( FAILED(hr) ) {
            throw std::runtime_error("Failed to create include handler");
        }
    }

    // HLSL�t�@�C����CSO�t�@�C���̍X�V���Ԃ��r
    WIN32_FILE_ATTRIBUTE_DATA hlslAttr, csoAttr;
    bool hlslExists = GetFileAttributesExW(hlslPath.c_str(), GetFileExInfoStandard, &hlslAttr);
    bool csoExists = GetFileAttributesExW(csoPath.c_str(), GetFileExInfoStandard, &csoAttr);

    bool needsRecompile = !csoExists || !hlslExists;
    if ( hlslExists && csoExists ) {
        // HLSL�t�@�C����CSO�t�@�C�����V�����ꍇ�͍ăR���p�C��
        FILETIME hlslTime = hlslAttr.ftLastWriteTime;
        FILETIME csoTime = csoAttr.ftLastWriteTime;
        needsRecompile = CompareFileTime(&hlslTime, &csoTime) > 0;
    }

    if ( needsRecompile && hlslExists ) {
        std::wcout << L"Compiling shader: " << hlslPath << L" -> " << csoPath << std::endl;

        // HLSL�t�@�C�����R���p�C��
        ComPtr<IDxcBlob> compiledBlob = CompileShaderFromFile(hlslPath, entryPoint, target);

        // CSO�t�@�C���ɕۑ�
        std::ofstream csoFile(csoPath, std::ios::binary);
        if ( csoFile.is_open() ) {
            csoFile.write(
                static_cast<const char*>( compiledBlob->GetBufferPointer() ),
                compiledBlob->GetBufferSize()
            );
            csoFile.close();
            std::wcout << L"Saved compiled shader: " << csoPath << std::endl;
        }

        return compiledBlob;
    }
    else if ( csoExists ) {
        // ������CSO�t�@�C����ǂݍ���
        std::wcout << L"Loading cached shader: " << csoPath << std::endl;
        return LoadCSO(csoPath);
    }
    else {
        throw std::runtime_error("Neither HLSL source nor CSO file found");
    }
}

void DXRRenderer::CreateOutputResource() {
    // ���C�g���[�V���O�o�͗pUAV�e�N�X�`���쐬
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC outputResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    HRESULT hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &outputResourceDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_raytracingOutput)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create raytracing output resource");
    }

    // ������ �f�B�X�N���v�^�q�[�v�쐬�iUAV 1�� + SRV 4�j ������
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.NumDescriptors = 5;  // UAV(1) + SRV(4)
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr = m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create descriptor heap");
    }

    // UAV�f�B�X�N���v�^�쐬�i�o�̓e�N�X�`���j
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_device->CreateUnorderedAccessView(
        m_raytracingOutput.Get(),
        nullptr,
        &uavDesc,
        m_descriptorHeap->GetCPUDescriptorHandleForHeapStart()
    );

    // �V�[���萔�o�b�t�@�쐬
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneConstantBuffer));

    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_sceneConstantBuffer)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create scene constant buffer");
    }
}

void DXRRenderer::CreateMaterialBuffer(const TLASData& tlasData) {
    if ( tlasData.blasDataList.empty() ) return;

    // �}�e���A���f�[�^��z��ɕϊ�
    std::vector<DXRMaterialData> materials;
    for ( const auto& blasData : tlasData.blasDataList ) {
        materials.push_back(blasData.material);

        // �f�o�b�O�o��
        char debugMsg[256];
        sprintf_s(debugMsg, "Material[%zu]: albedo=(%.3f,%.3f,%.3f), type=%d\n",
            materials.size() - 1, blasData.material.albedo.x,
            blasData.material.albedo.y, blasData.material.albedo.z,
            blasData.material.materialType);
        OutputDebugStringA(debugMsg);
    }

    // �}�e���A���o�b�t�@�쐬
    UINT materialBufferSize = static_cast<UINT>( materials.size() * sizeof(DXRMaterialData) );

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC materialBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize);

    HRESULT hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &materialBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_materialBuffer)
    );

    if ( SUCCEEDED(hr) ) {
        void* mappedData;
        m_materialBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, materials.data(), materialBufferSize);
        m_materialBuffer->Unmap(0, nullptr);

        char debugMsg[256];
        sprintf_s(debugMsg, "Material buffer created: %zu materials, size: %u bytes\n",
            materials.size(), materialBufferSize);
        OutputDebugStringA(debugMsg);
    }
}

void DXRRenderer::CreateShaderTables() {
    // �V�F�[�_�[���ʎq�擾
    ComPtr<ID3D12StateObjectProperties> stateObjectProps;
    HRESULT hr = m_rtStateObject.As(&stateObjectProps);
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to get state object properties");
    }

    // RayGeneration �V�F�[�_�[�e�[�u���쐬
    void* rayGenShaderID = stateObjectProps->GetShaderIdentifier(L"RayGen");
    if ( !rayGenShaderID ) {
        throw std::runtime_error("Failed to get RayGen shader identifier");
    }

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC rayGenShaderTableDesc = CD3DX12_RESOURCE_DESC::Buffer(s_shaderTableEntrySize);

    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &rayGenShaderTableDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_rayGenShaderTable)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create RayGen shader table");
    }

    void* mappedRayGenData;
    m_rayGenShaderTable->Map(0, nullptr, &mappedRayGenData);
    memcpy(mappedRayGenData, rayGenShaderID, s_shaderIdentifierSize);
    m_rayGenShaderTable->Unmap(0, nullptr);

    // Miss �V�F�[�_�[�e�[�u���쐬
    void* missShaderID = stateObjectProps->GetShaderIdentifier(L"Miss");
    if ( !missShaderID ) {
        throw std::runtime_error("Failed to get Miss shader identifier");
    }

    CD3DX12_RESOURCE_DESC missShaderTableDesc = CD3DX12_RESOURCE_DESC::Buffer(s_shaderTableEntrySize);

    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &missShaderTableDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_missShaderTable)
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create Miss shader table");
    }

    void* mappedMissData;
    m_missShaderTable->Map(0, nullptr, &mappedMissData);
    memcpy(mappedMissData, missShaderID, s_shaderIdentifierSize);
    m_missShaderTable->Unmap(0, nullptr);

    // �� �q�b�g�O���[�v�V�F�[�_�[�e�[�u���i�C���Łj
    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );
    if ( !dxrScene ) {
        throw std::runtime_error("No DXRScene found for shader table creation");
    }

    TLASData tlasData = dxrScene->GetTLASData();
    size_t numInstances = tlasData.blasDataList.size();

    // �e�}�e���A���^�C�v�̃V�F�[�_�[ID���擾
    void* lambertianHitGroupID = stateObjectProps->GetShaderIdentifier(L"HitGroup_Lambertian");
    void* metalHitGroupID = stateObjectProps->GetShaderIdentifier(L"HitGroup_Metal");
    void* dielectricHitGroupID = stateObjectProps->GetShaderIdentifier(L"HitGroup_Dielectric");
    void* lightHitGroupID = stateObjectProps->GetShaderIdentifier(L"HitGroup_DiffuseLight");

    // �}�e���A���^�C�v�ʂ̃V�F�[�_�[ID�z��
    void* materialShaderIDs[4] = {
        lambertianHitGroupID,    // �}�e���A���^�C�v 0
        metalHitGroupID,         // �}�e���A���^�C�v 1  
        dielectricHitGroupID,    // �}�e���A���^�C�v 2
        lightHitGroupID          // �}�e���A���^�C�v 3
    };

    // �q�b�g�O���[�v�e�[�u����4�G���g���i�}�e���A���^�C�v���j�ō쐬
    UINT hitGroupEntrySize = AlignTo(s_shaderIdentifierSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    CD3DX12_RESOURCE_DESC hitGroupShaderTableDesc = CD3DX12_RESOURCE_DESC::Buffer(hitGroupEntrySize * 4);

    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &hitGroupShaderTableDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_hitGroupShaderTable)
    );

    void* mappedHitGroupData;
    m_hitGroupShaderTable->Map(0, nullptr, &mappedHitGroupData);

    // �}�e���A���^�C�v���ɃV�F�[�_�[ID��z�u
    for ( int materialType = 0; materialType < 4; ++materialType ) {
        char* entryStart = static_cast<char*>( mappedHitGroupData ) + ( materialType * hitGroupEntrySize );
        memcpy(entryStart, materialShaderIDs[materialType], s_shaderIdentifierSize);

        char debugMsg[256];
        sprintf_s(debugMsg, "ShaderTable[%d]: MaterialType=%d, ShaderID=%p\n",
            materialType, materialType, materialShaderIDs[materialType]);
        OutputDebugStringA(debugMsg);
    }

    m_hitGroupShaderTable->Unmap(0, nullptr);
    s_hitGroupEntrySize = hitGroupEntrySize;

    /*
    // �q�b�g�O���[�v�e�[�u���쐬
    UINT hitGroupEntrySize = AlignTo(s_shaderIdentifierSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    CD3DX12_RESOURCE_DESC hitGroupShaderTableDesc = CD3DX12_RESOURCE_DESC::Buffer(hitGroupEntrySize * numInstances);

    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &hitGroupShaderTableDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_hitGroupShaderTable)
    );

    void* mappedHitGroupData;
    m_hitGroupShaderTable->Map(0, nullptr, &mappedHitGroupData);

    for ( size_t i = 0; i < numInstances; ++i ) {
        char* entryStart = static_cast<char*>( mappedHitGroupData ) + ( i * hitGroupEntrySize );

        // �� �}�e���A���^�C�v�ɉ����ăV�F�[�_�[ID��I��
        void* shaderID = nullptr;
        switch ( tlasData.blasDataList[i].material.materialType ) {
            case 0: shaderID = lambertianHitGroupID; break;   // Lambertian
            case 1: shaderID = metalHitGroupID; break;        // Metal
            case 2: shaderID = dielectricHitGroupID; break;   // Dielectric
            case 3: shaderID = lightHitGroupID; break;        // DiffuseLight
            default: shaderID = lambertianHitGroupID; break;  // �f�t�H���g
        }

        memcpy(entryStart, shaderID, s_shaderIdentifierSize);
    }

    m_hitGroupShaderTable->Unmap(0, nullptr);
    s_hitGroupEntrySize = hitGroupEntrySize;
    */
}