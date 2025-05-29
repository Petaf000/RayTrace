// DXRRenderer.cpp
#include "DXRRenderer.h"
#include "Renderer.h"
#include "GameManager.h"
#include "DXRScene.h"
#include "App.h"

void DXRRenderer::Init(Renderer* renderer) {
    // Rendererからdevice取得
    ID3D12Device* baseDevice = renderer->GetDevice();
    HRESULT hr = baseDevice->QueryInterface(IID_PPV_ARGS(&m_device));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create DXR Device");
    }

    // CommandQueueとCommandList取得
    m_commandQueue = renderer->GetCommandQueue();

    ComPtr<ID3D12GraphicsCommandList> baseCommandList = renderer->GetCommandList();
    hr = baseCommandList->QueryInterface(IID_PPV_ARGS(&m_commandList));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create DXR CommandList");
    }

    // サイズをRendererと完全に一致させる
    m_width = renderer->GetBufferWidth();
    m_height = renderer->GetBufferHeight();

    char debugMsg[256];
    sprintf_s(debugMsg, "=== DXR Init ===\n");
    OutputDebugStringA(debugMsg);
    sprintf_s(debugMsg, "Setting DXR size to match renderer: %ux%u\n", m_width, m_height);
    OutputDebugStringA(debugMsg);

    InitializeDXR(m_device.Get());
    CreateRootSignature();
    CreateLocalRootSignature();
    CreateMaterialConstantBuffers();
    CreateRaytracingPipelineStateObject();
    CreateOutputResource();
    CreateAccelerationStructures();
    CreateShaderTables();

    // ★ ImGui初期化は削除（Rendererが管理） ★
    // ImGui初期化コードを削除

    OutputDebugStringA("DXR initialization completed\n");
}

void DXRRenderer::UnInit() {
    // リソースのクリーンアップ
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

    // 現在のシーンを取得
    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );

    if ( !dxrScene ) {
        OutputDebugStringA("ERROR: No DXRScene found!\n");
        return;
    }

    UpdateCamera();

    // ★ サイズの確認と修正 ★
    auto& renderer = Singleton<Renderer>::getInstance();
    ID3D12Resource* currentBackBuffer = renderer.GetBackBuffer(renderer.GetCurrentFrameIndex());

    if ( !currentBackBuffer ) {
        OutputDebugStringA("ERROR: currentBackBuffer is NULL!\n");
        return;
    }

    // バックバッファのリソース記述を取得
    D3D12_RESOURCE_DESC backBufferDesc = currentBackBuffer->GetDesc();

    sprintf_s(debugMsg, "BackBuffer: %ux%u (format: %d)\n",
        (UINT)backBufferDesc.Width, backBufferDesc.Height, (int)backBufferDesc.Format);
    OutputDebugStringA(debugMsg);
    sprintf_s(debugMsg, "DXR current size: %ux%u\n", m_width, m_height);
    OutputDebugStringA(debugMsg);

    // ★ 4フレーム目以降：レイトレーシング実行 ★
    OutputDebugStringA("=== Raytracing Execution ===\n");

    // レイトレーシング実行
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
    raytraceDesc.HitGroupTable.SizeInBytes = s_hitGroupEntrySize;
    raytraceDesc.HitGroupTable.StrideInBytes = s_hitGroupEntrySize;

    // ★ ディスパッチ設定（サイズ確認済み） ★
    raytraceDesc.Width = m_width;
    raytraceDesc.Height = m_height;
    raytraceDesc.Depth = 1;

    sprintf_s(debugMsg, "About to dispatch rays: %ux%ux%u\n",
        raytraceDesc.Width, raytraceDesc.Height, raytraceDesc.Depth);
    OutputDebugStringA(debugMsg);

    // グローバルルートシグネチャ設定
    m_commandList->SetComputeRootSignature(m_globalRootSignature.Get());

    // DXR用ディスクリプタヒープ設定
    ID3D12DescriptorHeap* dxrHeaps[] = { m_descriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, dxrHeaps);

    // リソース設定
    m_commandList->SetComputeRootDescriptorTable(0, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    m_commandList->SetComputeRootShaderResourceView(1, m_topLevelAS->GetGPUVirtualAddress());
    m_commandList->SetComputeRootConstantBufferView(2, m_sceneConstantBuffer->GetGPUVirtualAddress());

    // レイトレーシング実行
    m_commandList->SetPipelineState1(m_rtStateObject.Get());

    OutputDebugStringA("Executing DispatchRays...\n");
    m_commandList->DispatchRays(&raytraceDesc);
    OutputDebugStringA("DispatchRays completed successfully!\n");

    // UAVバリア
    CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_raytracingOutput.Get());
    m_commandList->ResourceBarrier(1, &uavBarrier);

    // ★ レイトレーシング結果をバックバッファにコピー ★
    OutputDebugStringA("Starting raytracing result copy...\n");

    // リソース状態を遷移
    CD3DX12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
    };
    m_commandList->ResourceBarrier(2, barriers);

    // リソースコピー
    m_commandList->CopyResource(currentBackBuffer, m_raytracingOutput.Get());
    OutputDebugStringA("CopyResource completed\n");

    // リソース状態を復元
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
    // DXR機能の確認
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));

    if ( FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED ) {
        throw std::runtime_error("DXR is not supported on this device");
    }
}

void DXRRenderer::CreateRootSignature() {
    // グローバルルートシグネチャ作成
    CD3DX12_DESCRIPTOR_RANGE uavDescriptor;
    uavDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[3];
    rootParameters[0].InitAsDescriptorTable(1, &uavDescriptor); // 出力UAV
    rootParameters[1].InitAsShaderResourceView(0);              // TLAS
    rootParameters[2].InitAsConstantBufferView(0);              // 定数バッファ

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
    // シェーダー読み込み
    auto rayGenShader = LoadOrCompileShader(L"Src/Shader/RayGen.hlsl", L"RayGen");
    auto missShader = LoadOrCompileShader(L"Src/Shader/Miss.hlsl", L"Miss");
    auto lambertianHitShader = LoadOrCompileShader(L"Src/Shader/ClosestHit_Lambertian.hlsl", L"ClosestHit_Lambertian");
    auto metalHitShader = LoadOrCompileShader(L"Src/Shader/ClosestHit_Metal.hlsl", L"ClosestHit_Metal");
    auto dielectricHitShader = LoadOrCompileShader(L"Src/Shader/ClosestHit_Dielectric.hlsl", L"ClosestHit_Dielectric");
    auto lightHitShader = LoadOrCompileShader(L"Src/Shader/ClosestHit_DiffuseLight.hlsl", L"ClosestHit_DiffuseLight");

    // エクスポート名を固定文字列として定義
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

    // 固定サイズの配列を使用
    D3D12_EXPORT_DESC exportDescs[6];
    D3D12_DXIL_LIBRARY_DESC dxilLibDescs[6];
    D3D12_HIT_GROUP_DESC hitGroupDescs[4];
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfigDesc = {};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderConfigAssociationDesc = {};

    // ★★★ ローカルルートシグネチャ関連を追加 ★★★
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION localRootSigAssociationDesc = {};

    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.reserve(16); // 余裕を持ったサイズ

    // DXILライブラリサブオブジェクト作成
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

    // ヒットグループサブオブジェクト作成
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

    // シェーダー設定サブオブジェクト
    shaderConfigDesc.MaxPayloadSizeInBytes = ( 3 * sizeof(float) ) + ( 2 * sizeof(unsigned int) );
    shaderConfigDesc.MaxAttributeSizeInBytes = sizeof(float) * 2;

    D3D12_STATE_SUBOBJECT shaderConfigSubobject = {};
    shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigSubobject.pDesc = &shaderConfigDesc;
    subobjects.push_back(shaderConfigSubobject);

    // シェーダー設定をすべてのシェーダーエクスポートに関連付け
    shaderConfigAssociationDesc.pSubobjectToAssociate = &subobjects.back();
    shaderConfigAssociationDesc.NumExports = _countof(exportNames);
    shaderConfigAssociationDesc.pExports = exportNames;

    D3D12_STATE_SUBOBJECT shaderConfigAssociationSubobject = {};
    shaderConfigAssociationSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    shaderConfigAssociationSubobject.pDesc = &shaderConfigAssociationDesc;
    subobjects.push_back(shaderConfigAssociationSubobject);

    // ★★★ ローカルルートシグネチャサブオブジェクト ★★★
    D3D12_STATE_SUBOBJECT localRootSigSubobject = {};
    localRootSigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    localRootSigSubobject.pDesc = m_localRootSignature.GetAddressOf();
    subobjects.push_back(localRootSigSubobject);

    // ★★★ ローカルルートシグネチャをヒットグループに関連付け ★★★
    localRootSigAssociationDesc.pSubobjectToAssociate = &subobjects.back();
    localRootSigAssociationDesc.NumExports = _countof(hitGroupNames);
    localRootSigAssociationDesc.pExports = hitGroupNames;

    D3D12_STATE_SUBOBJECT localRootSigAssociationSubobject = {};
    localRootSigAssociationSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    localRootSigAssociationSubobject.pDesc = &localRootSigAssociationDesc;
    subobjects.push_back(localRootSigAssociationSubobject);

    // グローバルルートシグネチャ
    D3D12_STATE_SUBOBJECT globalRootSigSubobject = {};
    globalRootSigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    globalRootSigSubobject.pDesc = m_globalRootSignature.GetAddressOf();
    subobjects.push_back(globalRootSigSubobject);

    // パイプライン設定
    static D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = { 8 };

    D3D12_STATE_SUBOBJECT pipelineConfigSubobject = {};
    pipelineConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigSubobject.pDesc = &pipelineConfig;
    subobjects.push_back(pipelineConfigSubobject);

    // ステートオブジェクト作成
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
    // ファイルを開く
    std::ifstream file(filename, std::ios::binary);
    if ( !file.is_open() ) {
        std::string errorMsg = "Failed to open shader file: ";
        std::string filenameStr(filename.begin(), filename.end());
        throw std::runtime_error(errorMsg + filenameStr);
    }

    // ファイルサイズ取得
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // データ読み込み
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>( data.data() ), fileSize);
    file.close();

    if ( !m_dxcUtils ) {
        // もし m_dxcUtils がこの時点で初期化されていない場合のフォールバック処理
        // (本来は DXRRenderer の初期化時に行うべき)
        HRESULT hr_init = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_dxcUtils));
        if ( FAILED(hr_init) ) {
            throw std::runtime_error("Failed to create DxcUtils instance in LoadCSO");
        }
    }

    ComPtr<IDxcBlobEncoding> blobEncoding; // CreateBlob は IDxcBlobEncoding** を期待するため、この型で受ける
    HRESULT hr = m_dxcUtils->CreateBlob(
        data.data(),
        static_cast<UINT32>( data.size() ),
        DXC_CP_UTF8,
        blobEncoding.GetAddressOf() // 正しい ComPtr の使い方: IDxcBlobEncoding** を渡す
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create blob from shader data using IDxcUtils::CreateBlob");
    }

    ComPtr<IDxcBlob> resultBlob;
    // IDxcBlobEncoding から IDxcBlob インターフェースを取得 (QueryInterface に相当)
    // IDxcBlob は IDxcBlobEncoding を継承しているため、この変換は成功するはずです。
    hr = blobEncoding.As(&resultBlob);
    if ( FAILED(hr) ) {
        // このエラーは通常発生しにくいですが、念のためチェック
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

    cameraData.target.x++;

    // ビュー・プロジェクション行列計算
    XMVECTOR eyePos = XMLoadFloat3(&cameraData.position);
    XMVECTOR targetPos = XMLoadFloat3(&cameraData.target);
    XMVECTOR upVec = XMLoadFloat3(&cameraData.up);

    XMMATRIX viewMatrix = XMMatrixInverse(nullptr, XMMatrixLookAtLH(eyePos, targetPos, upVec));
    XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(cameraData.fov, cameraData.aspect, 0.1f, 1000.0f);

    // カメラからオブジェクトまでの距離計算
    float distToSphere1 = sqrtf(powf(190.0f - cameraData.position.x, 2) +
        powf(90.0f - cameraData.position.y, 2) +
        powf(190.0f - cameraData.position.z, 2));

    SceneConstantBuffer sceneConstants;
    sceneConstants.projectionMatrix = projMatrix;
    sceneConstants.viewMatrix = viewMatrix;
    sceneConstants.lightPosition = XMFLOAT4(0.0f, 554.0f, 279.5f, 1.0f);
    sceneConstants.lightColor = XMFLOAT4(15.0f, 15.0f, 15.0f, 1.0f);

    // 定数バッファ更新
    void* mappedData;
    m_sceneConstantBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, &sceneConstants, sizeof(SceneConstantBuffer));
    m_sceneConstantBuffer->Unmap(0, nullptr);

    OutputDebugStringA("Camera update completed\n");
}

// DXRRenderer_AccelerationStructure.cpp
// DXRRenderer.cppに追加する部分

void DXRRenderer::CreateAccelerationStructures() {
    // 現在のシーンからTLASデータ取得
    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );

    if ( !dxrScene ) {
        OutputDebugStringA("ERROR: No DXRScene found!\n");
        return;
    }

    TLASData tlasData = dxrScene->GetTLASData();

    // ★ 詳細なデバッグ情報 ★
    char debugMsg[512];
    sprintf_s(debugMsg, "=== Acceleration Structure Detailed Debug ===\n");
    OutputDebugStringA(debugMsg);
    sprintf_s(debugMsg, "Total BLAS count: %zu\n", tlasData.blasDataList.size());
    OutputDebugStringA(debugMsg);

    if ( tlasData.blasDataList.empty() ) {
        OutputDebugStringA("ERROR: No BLAS data found! No objects to render.\n");
        return;
    }

    for ( size_t i = 0; i < tlasData.blasDataList.size(); ++i ) {
        const auto& blasData = tlasData.blasDataList[i];
        sprintf_s(debugMsg, "=== BLAS[%zu] Detailed Analysis ===\n", i);
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "  Vertices: %zu, Indices: %zu\n",
            blasData.vertices.size(), blasData.indices.size());
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "  Material type: %d\n", blasData.material.materialType);
        OutputDebugStringA(debugMsg);

        if ( !blasData.vertices.empty() ) {
            // バウンディングボックス計算
            float minX = blasData.vertices[0].position.x;
            float maxX = blasData.vertices[0].position.x;
            float minY = blasData.vertices[0].position.y;
            float maxY = blasData.vertices[0].position.y;
            float minZ = blasData.vertices[0].position.z;
            float maxZ = blasData.vertices[0].position.z;

            for ( const auto& vertex : blasData.vertices ) {
                minX = min(minX, vertex.position.x);
                maxX = max(maxX, vertex.position.x);
                minY = min(minY, vertex.position.y);
                maxY = max(maxY, vertex.position.y);
                minZ = min(minZ, vertex.position.z);
                maxZ = max(maxZ, vertex.position.z);
            }

            sprintf_s(debugMsg, "  Bounding box: X[%.1f, %.1f], Y[%.1f, %.1f], Z[%.1f, %.1f]\n",
                minX, maxX, minY, maxY, minZ, maxZ);
            OutputDebugStringA(debugMsg);
            sprintf_s(debugMsg, "  Size: %.1f x %.1f x %.1f\n",
                maxX - minX, maxY - minY, maxZ - minZ);
            OutputDebugStringA(debugMsg);

            // ★ カメラから見える範囲かチェック ★
            float cameraX = 278.0f, cameraY = 278.0f, cameraZ = -600.0f;
            float targetX = 278.0f, targetY = 278.0f, targetZ = 278.0f;

            // オブジェクトがカメラの前方にあるか
            bool inFrontOfCamera = ( minZ > cameraZ );
            sprintf_s(debugMsg, "  In front of camera: %s\n", inFrontOfCamera ? "YES" : "NO");
            OutputDebugStringA(debugMsg);

            // オブジェクトがカメラの視線方向にあるか
            bool inViewDirection = ( minX <= targetX && maxX >= targetX ) &&
                ( minY <= targetY && maxY >= targetY );
            sprintf_s(debugMsg, "  In view direction: %s\n", inViewDirection ? "YES" : "NO");
            OutputDebugStringA(debugMsg);

            // オブジェクトの中心とカメラの距離
            float centerX = ( minX + maxX ) * 0.5f;
            float centerY = ( minY + maxY ) * 0.5f;
            float centerZ = ( minZ + maxZ ) * 0.5f;
            float distance = sqrtf(powf(centerX - cameraX, 2) +
                powf(centerY - cameraY, 2) +
                powf(centerZ - cameraZ, 2));
            sprintf_s(debugMsg, "  Distance from camera: %.1f\n", distance);
            OutputDebugStringA(debugMsg);
        }
    }

    // 既存のBLAS/TLAS作成処理...
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

    // BLAS構築完了後のバリア
    std::vector<CD3DX12_RESOURCE_BARRIER> blasBarriers;
    for ( auto& blas : m_bottomLevelAS ) {
        blasBarriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(blas.Get()));
    }
    if ( !blasBarriers.empty() ) {
        m_commandList->ResourceBarrier(static_cast<UINT>( blasBarriers.size() ), blasBarriers.data());
    }

    // TLAS作成
    OutputDebugStringA("Creating TLAS...\n");
    CreateTLAS(tlasData);

    Singleton<Renderer>::getInstance().ExecuteCommandListAndWait();
    OutputDebugStringA("Acceleration structures created successfully\n");
}

void DXRRenderer::CreateBLAS(BLASData& blasData, ComPtr<ID3D12Resource>& blasBuffer) {
    HRESULT hr;

    // ★ 頂点データの検証 ★
    if ( blasData.vertices.empty() || blasData.indices.empty() ) {
        OutputDebugStringA("ERROR: Empty vertices or indices in BLAS!\n");
        return;
    }

    char debugMsg[256];
    sprintf_s(debugMsg, "BLAS vertices: %zu, indices: %zu\n",
        blasData.vertices.size(), blasData.indices.size());
    OutputDebugStringA(debugMsg);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    // 頂点バッファ作成
    {
        UINT vertexBufferSize = static_cast<UINT>( blasData.vertices.size() * sizeof(DXRVertex) );
        sprintf_s(debugMsg, "Vertex buffer size: %u bytes\n", vertexBufferSize);
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
            OutputDebugStringA("ERROR: Failed to create vertex buffer for BLAS\n");
            throw std::runtime_error("Failed to create vertex buffer for BLAS");
        }

        // 頂点データをアップロード
        void* mappedVertexData;
        blasData.vertexBuffer->Map(0, nullptr, &mappedVertexData);
        memcpy(mappedVertexData, blasData.vertices.data(), vertexBufferSize);
        blasData.vertexBuffer->Unmap(0, nullptr);

        OutputDebugStringA("Vertex buffer created and data uploaded\n");
    }

    // インデックスバッファ作成
    {
        UINT indexBufferSize = static_cast<UINT>( blasData.indices.size() * sizeof(uint32_t) );
        sprintf_s(debugMsg, "Index buffer size: %u bytes\n", indexBufferSize);
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
            OutputDebugStringA("ERROR: Failed to create index buffer for BLAS\n");
            throw std::runtime_error("Failed to create index buffer for BLAS");
        }

        // インデックスデータをアップロード
        void* mappedIndexData;
        blasData.indexBuffer->Map(0, nullptr, &mappedIndexData);
        memcpy(mappedIndexData, blasData.indices.data(), indexBufferSize);
        blasData.indexBuffer->Unmap(0, nullptr);

        OutputDebugStringA("Index buffer created and data uploaded\n");
    }

    // ジオメトリディスクリプタ設定
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    // 頂点データ設定
    geometryDesc.Triangles.VertexBuffer.StartAddress = blasData.vertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(DXRVertex);
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount = static_cast<UINT>( blasData.vertices.size() );

    // インデックスデータ設定
    geometryDesc.Triangles.IndexBuffer = blasData.indexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geometryDesc.Triangles.IndexCount = static_cast<UINT>( blasData.indices.size() );

    sprintf_s(debugMsg, "Geometry setup: VertexCount=%u, IndexCount=%u\n",
        geometryDesc.Triangles.VertexCount, geometryDesc.Triangles.IndexCount);
    OutputDebugStringA(debugMsg);

    // BLAS入力設定
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
    blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    blasInputs.NumDescs = 1;
    blasInputs.pGeometryDescs = &geometryDesc;

    // BLAS前処理情報取得
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuildInfo);

    // BLASバッファ作成
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

    // スクラッチバッファ作成
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

    // BLAS構築実行（コマンドリストに記録するだけ）
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
    blasDesc.Inputs = blasInputs;
    blasDesc.DestAccelerationStructureData = blasBuffer->GetGPUVirtualAddress();
    blasDesc.ScratchAccelerationStructureData = blasData.scratchBuffer->GetGPUVirtualAddress();

    m_commandList->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);
}

void DXRRenderer::CreateTLAS(TLASData& tlasData) {
    // インスタンスディスクリプタ作成
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

    for ( size_t i = 0; i < tlasData.blasDataList.size(); ++i ) {
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};

        // ★修正：BLASDataから実際のワールド変換行列を使用
        XMMATRIX transform = tlasData.blasDataList[i].transform;

        // 3x4行列としてコピー（DXRのインスタンス変換は3x4行列）
        XMFLOAT3X4 transform3x4;
        XMStoreFloat3x4(&transform3x4, transform);
        memcpy(instanceDesc.Transform, &transform3x4, sizeof(instanceDesc.Transform));

        instanceDesc.InstanceID = static_cast<UINT>( i );
        instanceDesc.InstanceMask = 0xFF;
        instanceDesc.InstanceContributionToHitGroupIndex = static_cast<UINT>( tlasData.blasDataList[i].material.materialType );
        instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc.AccelerationStructure = m_bottomLevelAS[i]->GetGPUVirtualAddress();

        instanceDescs.push_back(instanceDesc);
    }

    // 以下のインスタンスバッファ作成以降のコードは既存のまま
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

    // インスタンスデータをアップロード
    void* mappedInstanceData;
    tlasData.instanceBuffer->Map(0, nullptr, &mappedInstanceData);
    memcpy(mappedInstanceData, instanceDescs.data(), instanceBufferSize);
    tlasData.instanceBuffer->Unmap(0, nullptr);

    // TLAS入力設定
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    tlasInputs.NumDescs = static_cast<UINT>( instanceDescs.size() );
    tlasInputs.InstanceDescs = tlasData.instanceBuffer->GetGPUVirtualAddress();

    // TLAS前処理情報取得
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

    // TLASバッファ作成
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

    // TLASスクラッチバッファ作成
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

    // TLAS構築実行（コマンドリストに記録するだけ）
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {};
    tlasDesc.Inputs = tlasInputs;
    tlasDesc.DestAccelerationStructureData = m_topLevelAS->GetGPUVirtualAddress();
    tlasDesc.ScratchAccelerationStructureData = m_topLevelASScratch->GetGPUVirtualAddress();

    m_commandList->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);

    // TLASバリア
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_topLevelAS.Get());
    m_commandList->ResourceBarrier(1, &barrier);
}

ComPtr<IDxcBlob> DXRRenderer::CompileShaderFromFile(const std::wstring& hlslPath, const std::wstring& entryPoint, const std::wstring& target) {
    // HLSLファイルを読み込み
    ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = m_dxcUtils->LoadFile(hlslPath.c_str(), nullptr, &sourceBlob);
    if ( FAILED(hr) ) {
        std::string errorMsg = "Failed to load HLSL file: ";
        std::string filenameStr(hlslPath.begin(), hlslPath.end());
        throw std::runtime_error(errorMsg + filenameStr);
    }

    // コンパイル引数の設定
    std::vector<LPCWSTR> arguments = {
        hlslPath.c_str(),               // ファイル名（デバッグ用）
        L"-E", entryPoint.c_str(),      // エントリーポイント
        L"-T", target.c_str(),          // ターゲット（lib_6_5など）
        L"-O3",                         // 最適化レベル
        L"-Qstrip_debug",               // デバッグ情報を削除
        L"-Qstrip_reflect",             // リフレクション情報を削除
    };

    // DXCBufferを作成
    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = CP_UTF8;

    // コンパイル実行
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

    // コンパイル結果の確認
    HRESULT compileHR;
    compileResult->GetStatus(&compileHR);

    if ( FAILED(compileHR) ) {
        // エラーメッセージを取得
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

    // コンパイル済みシェーダーを取得
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

    // DXCコンパイラーの初期化（一度だけ）
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

    // HLSLファイルとCSOファイルの更新時間を比較
    WIN32_FILE_ATTRIBUTE_DATA hlslAttr, csoAttr;
    bool hlslExists = GetFileAttributesExW(hlslPath.c_str(), GetFileExInfoStandard, &hlslAttr);
    bool csoExists = GetFileAttributesExW(csoPath.c_str(), GetFileExInfoStandard, &csoAttr);

    bool needsRecompile = !csoExists || !hlslExists;
    if ( hlslExists && csoExists ) {
        // HLSLファイルがCSOファイルより新しい場合は再コンパイル
        FILETIME hlslTime = hlslAttr.ftLastWriteTime;
        FILETIME csoTime = csoAttr.ftLastWriteTime;
        needsRecompile = CompareFileTime(&hlslTime, &csoTime) > 0;
    }

    if ( needsRecompile && hlslExists ) {
        std::wcout << L"Compiling shader: " << hlslPath << L" -> " << csoPath << std::endl;

        // HLSLファイルをコンパイル
        ComPtr<IDxcBlob> compiledBlob = CompileShaderFromFile(hlslPath, entryPoint, target);

        // CSOファイルに保存
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
        // 既存のCSOファイルを読み込み
        std::wcout << L"Loading cached shader: " << csoPath << std::endl;
        return LoadCSO(csoPath);
    }
    else {
        throw std::runtime_error("Neither HLSL source nor CSO file found");
    }
}

void DXRRenderer::CreateOutputResource() {
    // レイトレーシング出力用UAVテクスチャ作成
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

    // ディスクリプタヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.NumDescriptors = 1;
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr = m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create descriptor heap");
    }

    // UAVディスクリプタ作成
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    m_device->CreateUnorderedAccessView(
        m_raytracingOutput.Get(),
        nullptr,
        &uavDesc,
        m_descriptorHeap->GetCPUDescriptorHandleForHeapStart()
    );

    // 定数バッファ作成
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

void DXRRenderer::CreateLocalRootSignature() {
    // ローカルルートシグネチャ作成（マテリアル用定数バッファ）
    CD3DX12_ROOT_PARAMETER localRootParameter;
    localRootParameter.InitAsConstantBufferView(1, 1); // register(b1, space1)

    CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(1, &localRootParameter);
    localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&localRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

    if ( FAILED(hr) ) {
        if ( error ) {
            std::string errorMsg(static_cast<char*>( error->GetBufferPointer() ));
            throw std::runtime_error("Failed to serialize local root signature: " + errorMsg);
        }
        throw std::runtime_error("Failed to serialize local root signature");
    }

    hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_localRootSignature));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create local root signature");
    }
}

void DXRRenderer::CreateMaterialConstantBuffers() {
    // 4つのマテリアル用定数バッファを作成
    m_materialConstantBuffers.resize(4);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    UINT alignedSize = AlignTo(sizeof(DXRMaterialData), 256);
    CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);

    for ( int i = 0; i < 4; ++i ) {
        HRESULT hr = m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &constantBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_materialConstantBuffers[i])
        );

        if ( FAILED(hr) ) {
            throw std::runtime_error("Failed to create material constant buffer");
        }
    }

    // マテリアルデータを設定
    UpdateMaterialData();
}

void DXRRenderer::UpdateMaterialData() {
    // 現在のシーンからマテリアルデータを取得
    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );

    if ( !dxrScene ) return;

    auto dxrShapes = dxrScene->GetDXRShapes();

    // 各マテリアルタイプのデフォルトデータを設定
    std::vector<DXRMaterialData> defaultMaterials(4);

    // Lambertian (白)
    defaultMaterials[0].albedo = { 0.73f, 0.73f, 0.73f };
    defaultMaterials[0].roughness = 1.0f;
    defaultMaterials[0].refractiveIndex = 1.0f;
    defaultMaterials[0].emission = { 0.0f, 0.0f, 0.0f };
    defaultMaterials[0].materialType = 0;

    // Metal (アルミニウム)
    defaultMaterials[1].albedo = { 0.8f, 0.85f, 0.88f };
    defaultMaterials[1].roughness = 0.0f;
    defaultMaterials[1].refractiveIndex = 1.0f;
    defaultMaterials[1].emission = { 0.0f, 0.0f, 0.0f };
    defaultMaterials[1].materialType = 1;

    // Dielectric (ガラス)
    defaultMaterials[2].albedo = { 1.0f, 1.0f, 1.0f };
    defaultMaterials[2].roughness = 0.0f;
    defaultMaterials[2].refractiveIndex = 1.5f;
    defaultMaterials[2].emission = { 0.0f, 0.0f, 0.0f };
    defaultMaterials[2].materialType = 2;

    // DiffuseLight (発光)
    defaultMaterials[3].albedo = { 1.0f, 1.0f, 1.0f };
    defaultMaterials[3].roughness = 0.0f;
    defaultMaterials[3].refractiveIndex = 1.0f;
    defaultMaterials[3].emission = { 15.0f, 15.0f, 15.0f };
    defaultMaterials[3].materialType = 3;

    // 各定数バッファにデータを設定
    for ( int i = 0; i < 4; ++i ) {
        void* mappedData;
        m_materialConstantBuffers[i]->Map(0, nullptr, &mappedData);
        memcpy(mappedData, &defaultMaterials[i], sizeof(DXRMaterialData));
        m_materialConstantBuffers[i]->Unmap(0, nullptr);
    }
}

void DXRRenderer::CreateShaderTables() {
    // シェーダー識別子取得
    ComPtr<ID3D12StateObjectProperties> stateObjectProps;
    HRESULT hr = m_rtStateObject.As(&stateObjectProps);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to get state object properties");
    }

    // RayGeneration シェーダーテーブル作成
    void* rayGenShaderID = stateObjectProps->GetShaderIdentifier(L"RayGen");
    if (!rayGenShaderID) {
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

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create RayGen shader table");
    }

    void* mappedRayGenData;
    m_rayGenShaderTable->Map(0, nullptr, &mappedRayGenData);
    memcpy(mappedRayGenData, rayGenShaderID, s_shaderIdentifierSize);
    m_rayGenShaderTable->Unmap(0, nullptr);

    // Miss シェーダーテーブル作成
    void* missShaderID = stateObjectProps->GetShaderIdentifier(L"Miss");
    if (!missShaderID) {
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

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create Miss shader table");
    }

    void* mappedMissData;
    m_missShaderTable->Map(0, nullptr, &mappedMissData);
    memcpy(mappedMissData, missShaderID, s_shaderIdentifierSize);
    m_missShaderTable->Unmap(0, nullptr);

    // ★★★ Hit Group シェーダーテーブル作成（ローカルルートシグネチャ対応） ★★★
    const wchar_t* hitGroupNames[] = {
        L"HitGroup_Lambertian",
        L"HitGroup_Metal",
        L"HitGroup_Dielectric",
        L"HitGroup_DiffuseLight"
    };

    // ヒットグループエントリのサイズ = シェーダーID + ローカルルートシグネチャデータ
    UINT hitGroupEntrySize = AlignTo(s_shaderIdentifierSize + sizeof(D3D12_GPU_VIRTUAL_ADDRESS), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    CD3DX12_RESOURCE_DESC hitGroupShaderTableDesc = CD3DX12_RESOURCE_DESC::Buffer(hitGroupEntrySize * 4);

    hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &hitGroupShaderTableDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_hitGroupShaderTable)
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create HitGroup shader table");
    }

    // 各ヒットグループのシェーダーIDとローカルルートシグネチャデータをコピー
    void* mappedHitGroupData;
    m_hitGroupShaderTable->Map(0, nullptr, &mappedHitGroupData);

    for (int i = 0; i < 4; ++i) {
        void* hitGroupShaderID = stateObjectProps->GetShaderIdentifier(hitGroupNames[i]);
        if (!hitGroupShaderID) {
            throw std::runtime_error("Hit group shader identifier not found");
        }

        char* entryStart = static_cast<char*>(mappedHitGroupData) + (i * hitGroupEntrySize);
        
        // シェーダーIDをコピー
        memcpy(entryStart, hitGroupShaderID, s_shaderIdentifierSize);
        
        // ローカルルートシグネチャデータ（マテリアル定数バッファのGPUアドレス）をコピー
        D3D12_GPU_VIRTUAL_ADDRESS materialCBAddress = m_materialConstantBuffers[i]->GetGPUVirtualAddress();
        memcpy(entryStart + s_shaderIdentifierSize, &materialCBAddress, sizeof(D3D12_GPU_VIRTUAL_ADDRESS));
    }

    m_hitGroupShaderTable->Unmap(0, nullptr);

    // シェーダーテーブルエントリサイズを更新
    s_hitGroupEntrySize = hitGroupEntrySize; // クラスメンバーとして追加が必要
}