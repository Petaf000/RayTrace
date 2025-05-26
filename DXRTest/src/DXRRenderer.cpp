#include "DXRRenderer.h"


DXRRenderer::DXRRenderer()
    : m_hwnd(nullptr)
    , m_width(0)
    , m_height(0)
    , m_frameIndex(0)
    , m_fenceValue(0)
    , m_fenceEvent(nullptr)
    , m_dxrSupported(false) {
}

DXRRenderer::~DXRRenderer() {
    UnInit();
}

bool DXRRenderer::Init(ID3D12Device5* device, ID3D12CommandQueue* commandQueue, HWND hwnd, int width, int height) {
    m_device = device;
    m_commandQueue = commandQueue;
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    // DXRサポートチェック
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    HRESULT hr = m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
    if ( FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED ) {
        return false;
    }
    m_dxrSupported = true;

    // スワップチェーン作成
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    ComPtr<IDXGISwapChain1> swapChain;
    hr = factory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain);
    if ( FAILED(hr) ) return false;

    hr = swapChain.As(&m_swapChain);
    if ( FAILED(hr) ) return false;

    // コマンドアロケータとリスト作成
    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
    if ( FAILED(hr) ) return false;

    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    if ( FAILED(hr) ) return false;

    m_commandList->Close();

    // フェンス作成
    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if ( FAILED(hr) ) return false;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if ( m_fenceEvent == nullptr ) return false;

    // レンダーターゲット作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
    if ( FAILED(hr) ) return false;

    m_renderTargets.resize(2);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for ( int i = 0; i < 2; i++ ) {
        hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        if ( FAILED(hr) ) return false;
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
    }

    // DXR初期化
    if ( !InitDXR() ) return false;
    if ( !CreateDescriptorHeaps() ) return false;
    if ( !CreateOutputResource() ) return false;
    if ( !LoadShaders() ) return false;
    if ( !CreateRootSignatures() ) return false;
    if ( !CreateRaytracingPipeline() ) return false;

    return true;
}

void DXRRenderer::UnInit() {
    if ( m_fenceEvent ) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}

bool DXRRenderer::InitDXR() {
    // 既にDevice5があるので、そのまま使用
    return true;
}

bool DXRRenderer::LoadShaders() {
    // シェーダーファイル読み込み
    auto loadShader = [] (const std::wstring& filename) -> ComPtr<ID3DBlob> {
        std::ifstream file(filename, std::ios::binary);
        if ( !file.is_open() ) return nullptr;

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        ComPtr<ID3DBlob> blob;
        D3DCreateBlob(size, &blob);
        file.read(static_cast<char*>( blob->GetBufferPointer() ), size);

        return blob;
        };

    m_raygenShader = loadShader(L"Shaders/RayGeneration.cso");
    m_missShader = loadShader(L"Shaders/Miss.cso");
    m_closestHitShader = loadShader(L"Shaders/ClosestHit.cso");

    return m_raygenShader && m_missShader && m_closestHitShader;
}

bool DXRRenderer::CreateRootSignatures() {
    // グローバルルートシグネチャ
    CD3DX12_DESCRIPTOR_RANGE ranges[6];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // 出力テクスチャ
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // TLAS
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // グローバル定数
    ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // マテリアルバッファ
    ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 2); // 頂点バッファ（unbounded）
    ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, -1, 3); // インデックスバッファ（unbounded）

    CD3DX12_ROOT_PARAMETER rootParams[6];
    rootParams[0].InitAsDescriptorTable(1, &ranges[0]);
    rootParams[1].InitAsDescriptorTable(1, &ranges[1]);
    rootParams[2].InitAsDescriptorTable(1, &ranges[2]);
    rootParams[3].InitAsDescriptorTable(1, &ranges[3]);
    rootParams[4].InitAsDescriptorTable(1, &ranges[4]);
    rootParams[5].InitAsDescriptorTable(1, &ranges[5]);

    CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc;
    globalRootSignatureDesc.Init(_countof(rootParams), rootParams);

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&globalRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if ( FAILED(hr) ) {
        if ( error ) {
            std::cerr << "Root signature serialization error: " << (char*)error->GetBufferPointer() << std::endl;
        }
        return false;
    }

    hr = m_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_globalRootSignature));
    if ( FAILED(hr) ) return false;

    // ローカルルートシグネチャ（空）
    CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc;
    localRootSignatureDesc.Init(0, nullptr);

    hr = D3D12SerializeRootSignature(&localRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if ( FAILED(hr) ) return false;

    hr = m_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_localRootSignature));
    return SUCCEEDED(hr);
}

bool DXRRenderer::CreateRaytracingPipeline() {
    // レイトレーシングパイプライン設定
    CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    // シェーダー設定
    auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(m_raygenShader.Get());
    lib->SetDXILLibrary(&libdxil);
    lib->DefineExport(L"RayGeneration");

    auto missLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE missdxil = CD3DX12_SHADER_BYTECODE(m_missShader.Get());
    missLib->SetDXILLibrary(&missdxil);
    missLib->DefineExport(L"Miss");

    auto hitLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE hitdxil = CD3DX12_SHADER_BYTECODE(m_closestHitShader.Get());
    hitLib->SetDXILLibrary(&hitdxil);
    hitLib->DefineExport(L"ClosestHit");

    // ヒットグループ
    auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroup->SetClosestHitShaderImport(L"ClosestHit");
    hitGroup->SetHitGroupExport(L"HitGroup");
    hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

    // レイトレーシング設定
    auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    UINT payloadSize = 4 * sizeof(float); // ray payload
    UINT attributeSize = 2 * sizeof(float); // triangle attributes
    shaderConfig->Config(payloadSize, attributeSize);

    // グローバルルートシグネチャ
    auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSignature->SetRootSignature(m_globalRootSignature.Get());

    // パイプライン設定
    auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    const UINT maxTraceRecursionDepth = 10;
    pipelineConfig->Config(maxTraceRecursionDepth);

    HRESULT hr = m_device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_raytracingPipeline));
    if ( FAILED(hr) ) return false;

    hr = m_raytracingPipeline->QueryInterface(IID_PPV_ARGS(&m_raytracingPipelineProperties));
    return SUCCEEDED(hr);
}

bool DXRRenderer::CreateDescriptorHeaps() {
    // 必要なデスクリプタ数を計算
    UINT numDescriptors = 4; // UAV + TLAS SRV + CBV + Material SRV
    numDescriptors += static_cast<UINT>( m_geometryVertices.size() ); // 頂点バッファSRV
    numDescriptors += static_cast<UINT>( m_geometryIndices.size() );  // インデックスバッファSRV

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    return SUCCEEDED(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeap)));
}

bool DXRRenderer::CreateOutputResource() {
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = m_width;
    desc.Height = m_height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_outputResource));
    if ( FAILED(hr) ) return false;

    // UAVデスクリプタ作成
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    m_device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, handle);

    return true;
}

bool DXRRenderer::CreateShaderBindingTable() {
    void* rayGenShaderID = m_raytracingPipelineProperties->GetShaderIdentifier(L"RayGeneration");
    void* missShaderID = m_raytracingPipelineProperties->GetShaderIdentifier(L"Miss");
    void* hitGroupID = m_raytracingPipelineProperties->GetShaderIdentifier(L"HitGroup");

    const UINT shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const UINT shaderRecordSize = shaderIDSize; // シェーダーIDのみ

    // SBTサイズ計算
    const UINT sbtSize = shaderRecordSize * 3; // RayGen + Miss + HitGroup

    // SBTバッファ作成
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sbtSize);

    HRESULT hr = m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_shaderBindingTable));
    if ( FAILED(hr) ) return false;

    // SBTデータ書き込み
    void* mappedData;
    hr = m_shaderBindingTable->Map(0, nullptr, &mappedData);
    if ( FAILED(hr) ) return false;

    uint8_t* data = static_cast<uint8_t*>( mappedData );

    // RayGeneration
    memcpy(data, rayGenShaderID, shaderIDSize);
    data += shaderRecordSize;

    // Miss
    memcpy(data, missShaderID, shaderIDSize);
    data += shaderRecordSize;

    // HitGroup
    memcpy(data, hitGroupID, shaderIDSize);

    m_shaderBindingTable->Unmap(0, nullptr);
    return true;
}

void DXRRenderer::SetMaterials(const std::vector<DXRMaterial>& materials) {
    m_materials = materials;

    // マテリアルバッファ作成
    if ( !materials.empty() ) {
        const UINT bufferSize = static_cast<UINT>( sizeof(DXRMaterial) * materials.size() );

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

        HRESULT hr = m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_materialBuffer));

        if ( SUCCEEDED(hr) ) {
            void* mappedData;
            m_materialBuffer->Map(0, nullptr, &mappedData);
            memcpy(mappedData, materials.data(), bufferSize);
            m_materialBuffer->Unmap(0, nullptr);

            // SRVデスクリプタ作成
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = static_cast<UINT>( materials.size() );
            srvDesc.Buffer.StructureByteStride = sizeof(DXRMaterial);

            CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(3, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            m_device->CreateShaderResourceView(m_materialBuffer.Get(), &srvDesc, handle);
        }
    }
}

void DXRRenderer::SetGeometries(const std::vector<std::vector<DXRVertex>>& vertices,
    const std::vector<std::vector<uint32_t>>& indices) {
    m_geometryVertices = vertices;
    m_geometryIndices = indices;

    m_vertexBuffers.clear();
    m_indexBuffers.clear();
    m_vertexBuffers.resize(vertices.size());
    m_indexBuffers.resize(indices.size());

    // 各ジオメトリのバッファ作成
    for ( size_t i = 0; i < vertices.size(); i++ ) {
        if ( !vertices[i].empty() ) {
            CreateGeometryBuffers(i, vertices[i], indices[i]);
        }
    }

    // 頂点・インデックスバッファ用のSRV作成
    CreateVertexIndexSRVs();
}

bool DXRRenderer::CreateGeometryBuffers(size_t geometryIndex,
    const std::vector<DXRVertex>& vertices,
    const std::vector<uint32_t>& indices) {
    // 頂点バッファ作成
    if ( !vertices.empty() ) {
        const UINT vertexBufferSize = static_cast<UINT>( sizeof(DXRVertex) * vertices.size() );

        CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

        HRESULT hr = m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vertexBuffers[geometryIndex]));
        if ( FAILED(hr) ) return false;

        // アップロードヒープ経由でデータコピー
        if ( !UploadBufferData(m_vertexBuffers[geometryIndex].Get(), vertices.data(), vertexBufferSize) ) {
            return false;
        }
    }

    // インデックスバッファ作成
    if ( !indices.empty() ) {
        const UINT indexBufferSize = static_cast<UINT>( sizeof(uint32_t) * indices.size() );

        CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

        HRESULT hr = m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &indexBufferDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_indexBuffers[geometryIndex]));
        if ( FAILED(hr) ) return false;

        // アップロードヒープ経由でデータコピー
        if ( !UploadBufferData(m_indexBuffers[geometryIndex].Get(), indices.data(), indexBufferSize) ) {
            return false;
        }
    }

    return true;
}

bool DXRRenderer::UploadBufferData(ID3D12Resource* destBuffer, const void* data, UINT dataSize) {
    // アップロードヒープ作成
    ComPtr<ID3D12Resource> uploadBuffer;
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

    HRESULT hr = m_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));
    if ( FAILED(hr) ) return false;

    // データマップ
    void* mappedData;
    hr = uploadBuffer->Map(0, nullptr, &mappedData);
    if ( FAILED(hr) ) return false;

    memcpy(mappedData, data, dataSize);
    uploadBuffer->Unmap(0, nullptr);

    // コピーコマンド実行
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

    // リソース状態遷移
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        destBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->CopyResource(destBuffer, uploadBuffer.Get());

    // リソース状態を戻す
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        destBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->Close();

    m_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)m_commandList.GetAddressOf());

    // 同期待ち
    WaitForGPU();

    return true;
}

void DXRRenderer::CreateVertexIndexSRVs() {
    // 頂点バッファ用SRV作成
    if ( !m_vertexBuffers.empty() ) {
        for ( size_t i = 0; i < m_vertexBuffers.size(); i++ ) {
            if ( m_vertexBuffers[i] ) {
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Buffer.FirstElement = 0;
                srvDesc.Buffer.NumElements = static_cast<UINT>( m_geometryVertices[i].size() );
                srvDesc.Buffer.StructureByteStride = sizeof(DXRVertex);

                // デスクリプタヒープの適切な位置に作成
                CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
                handle.Offset(4 + static_cast<INT>( i ), m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
                m_device->CreateShaderResourceView(m_vertexBuffers[i].Get(), &srvDesc, handle);
            }
        }
    }

    // インデックスバッファ用SRV作成
    if ( !m_indexBuffers.empty() ) {
        for ( size_t i = 0; i < m_indexBuffers.size(); i++ ) {
            if ( m_indexBuffers[i] ) {
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = DXGI_FORMAT_R32_UINT;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Buffer.FirstElement = 0;
                srvDesc.Buffer.NumElements = static_cast<UINT>( m_geometryIndices[i].size() );

                // デスクリプタヒープの適切な位置に作成
                CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
                handle.Offset(4 + static_cast<INT>( m_vertexBuffers.size() + i ),
                    m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
                m_device->CreateShaderResourceView(m_indexBuffers[i].Get(), &srvDesc, handle);
            }
        }
    }
}

void DXRRenderer::SetInstances(const std::vector<DXRInstance>& instances) {
    m_instances = instances;
}

void DXRRenderer::SetCamera(const DXRCameraData& camera) {
    m_camera = camera;
}

void DXRRenderer::SetGlobalConstants(const DXRGlobalConstants& constants) {
    m_globalConstants = constants;

    // グローバル定数バッファ更新
    if ( !m_globalConstantBuffer ) {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(DXRGlobalConstants));

        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_globalConstantBuffer));

        // CBVデスクリプタ作成
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_globalConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = AlignUp(sizeof(DXRGlobalConstants), 256);

        CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(2, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        m_device->CreateConstantBufferView(&cbvDesc, handle);
    }

    void* mappedData;
    m_globalConstantBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, &constants, sizeof(DXRGlobalConstants));
    m_globalConstantBuffer->Unmap(0, nullptr);
}

void DXRRenderer::BuildBLAS() {
    m_bottomLevelAS.clear();
    m_bottomLevelAS.resize(m_geometryVertices.size());

    for ( size_t i = 0; i < m_geometryVertices.size(); i++ ) {
        if ( m_geometryVertices[i].empty() ) continue;

        // ジオメトリ記述
        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffers[i]->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(DXRVertex);
        geometryDesc.Triangles.VertexCount = static_cast<UINT>( m_geometryVertices[i].size() );
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.IndexBuffer = m_indexBuffers[i]->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = static_cast<UINT>( m_geometryIndices[i].size() );
        geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

        // BLAS入力
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
        blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        blasInputs.NumDescs = 1;
        blasInputs.pGeometryDescs = &geometryDesc;

        // 必要なサイズ取得
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo;
        m_device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuildInfo);

        // スクラッチバッファ作成
        ComPtr<ID3D12Resource> scratchBuffer = CreateBuffer(
            blasPrebuildInfo.ScratchDataSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT));

        // BLASバッファ作成
        m_bottomLevelAS[i] = CreateBuffer(
            blasPrebuildInfo.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT));

        // BLAS構築
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
        blasDesc.Inputs = blasInputs;
        blasDesc.ScratchDataAddress = scratchBuffer->GetGPUVirtualAddress();
        blasDesc.DestAccelerationStructureData = m_bottomLevelAS[i]->GetGPUVirtualAddress();

        m_commandAllocator->Reset();
        m_commandList->Reset(m_commandAllocator.Get(), nullptr);
        m_commandList->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);

        // バリア
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_bottomLevelAS[i].Get());
        m_commandList->ResourceBarrier(1, &barrier);

        m_commandList->Close();
        m_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)m_commandList.GetAddressOf());

        // 同期
        m_fenceValue++;
        m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
        if ( m_fence->GetCompletedValue() < m_fenceValue ) {
            m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
}

void DXRRenderer::BuildTLAS() {
    if ( m_instances.empty() || m_bottomLevelAS.empty() ) return;

    // インスタンスバッファ作成
    const UINT instanceBufferSize = static_cast<UINT>( sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * m_instances.size() );

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(instanceBufferSize);

    m_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_instanceBuffer));

    // インスタンスデータ設定
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
    m_instanceBuffer->Map(0, nullptr, (void**)&instanceDescs);

    for ( size_t i = 0; i < m_instances.size(); i++ ) {
        instanceDescs[i] = {};

        // ワールド行列設定（転置）
        XMMATRIX worldMatrix = m_instances[i].worldMatrix;
        worldMatrix = XMMatrixTranspose(worldMatrix);
        memcpy(instanceDescs[i].Transform, &worldMatrix, sizeof(instanceDescs[i].Transform));

        instanceDescs[i].InstanceID = static_cast<UINT>( i );
        instanceDescs[i].InstanceMask = 0xFF;
        instanceDescs[i].InstanceContributionToHitGroupIndex = 0;
        instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

        // BLAS参照
        int geometryIndex = m_instances[i].geometryIndex;
        if ( geometryIndex < m_bottomLevelAS.size() && m_bottomLevelAS[geometryIndex] ) {
            instanceDescs[i].AccelerationStructure = m_bottomLevelAS[geometryIndex]->GetGPUVirtualAddress();
        }
    }

    m_instanceBuffer->Unmap(0, nullptr);

    // TLAS入力
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlasInputs.NumDescs = static_cast<UINT>( m_instances.size() );
    tlasInputs.InstanceDescs = m_instanceBuffer->GetGPUVirtualAddress();

    // 必要なサイズ取得
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo;
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

    // スクラッチバッファ作成
    ComPtr<ID3D12Resource> scratchBuffer = CreateBuffer(
        tlasPrebuildInfo.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT));

    // TLASバッファ作成
    m_topLevelAS = CreateBuffer(
        tlasPrebuildInfo.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT));

    // TLAS構築
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {};
    tlasDesc.Inputs = tlasInputs;
    tlasDesc.ScratchDataAddress = scratchBuffer->GetGPUVirtualAddress();
    tlasDesc.DestAccelerationStructureData = m_topLevelAS->GetGPUVirtualAddress();

    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    m_commandList->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);

    // バリア
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_topLevelAS.Get());
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->Close();
    m_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)m_commandList.GetAddressOf());

    // TLAS用SRV作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_topLevelAS->GetGPUVirtualAddress();

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_device->CreateShaderResourceView(nullptr, &srvDesc, handle);

    // 同期
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    if ( m_fence->GetCompletedValue() < m_fenceValue ) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void DXRRenderer::Render() {
    if ( !CreateShaderBindingTable() ) return;

    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

    // パイプライン設定
    m_commandList->SetComputeRootSignature(m_globalRootSignature.Get());
    m_commandList->SetPipelineState1(m_raytracingPipeline.Get());

    // デスクリプタヒープ設定
    ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // ルートパラメータ設定
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    m_commandList->SetComputeRootDescriptorTable(0, handle); // Output
    handle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_commandList->SetComputeRootDescriptorTable(1, handle); // TLAS
    handle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_commandList->SetComputeRootDescriptorTable(2, handle); // Constants
    handle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_commandList->SetComputeRootDescriptorTable(3, handle); // Materials
    handle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_commandList->SetComputeRootDescriptorTable(4, handle); // Vertices
    handle.Offset(static_cast<INT>( m_vertexBuffers.size() ), m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_commandList->SetComputeRootDescriptorTable(5, handle); // Indices

    // 残りのレンダリング処理は既存と同じ...

    // レイトレーシング実行
    D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};

    // RayGeneration
    raytraceDesc.RayGenerationShaderRecord.StartAddress = m_shaderBindingTable->GetGPUVirtualAddress();
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    // Miss
    UINT shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    raytraceDesc.MissShaderTable.StartAddress = m_shaderBindingTable->GetGPUVirtualAddress() + shaderRecordSize;
    raytraceDesc.MissShaderTable.SizeInBytes = shaderRecordSize;
    raytraceDesc.MissShaderTable.StrideInBytes = shaderRecordSize;

    // HitGroup
    raytraceDesc.HitGroupTable.StartAddress = m_shaderBindingTable->GetGPUVirtualAddress() + shaderRecordSize * 2;
    raytraceDesc.HitGroupTable.SizeInBytes = shaderRecordSize;
    raytraceDesc.HitGroupTable.StrideInBytes = shaderRecordSize;

    raytraceDesc.Width = m_width;
    raytraceDesc.Height = m_height;
    raytraceDesc.Depth = 1;

    m_commandList->DispatchRays(&raytraceDesc);

    // 出力リソースをレンダーターゲットにコピー
    CD3DX12_RESOURCE_BARRIER barriers[2];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
    m_commandList->ResourceBarrier(2, barriers);

    m_commandList->CopyResource(m_renderTargets[m_frameIndex].Get(), m_outputResource.Get());

    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(2, barriers);

    m_commandList->Close();

    m_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)m_commandList.GetAddressOf());

    // プレゼント
    m_swapChain->Present(1, 0);

    // フレーム同期
    WaitForGPU();
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DXRRenderer::ResizeBuffers(int width, int height) {
    m_width = width;
    m_height = height;

    // 出力リソース再作成
    m_outputResource.Reset();
    CreateOutputResource();
}

ComPtr<ID3D12Resource> DXRRenderer::CreateBuffer(UINT64 size, D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initState,
    const D3D12_HEAP_PROPERTIES& heapProps) {
    ComPtr<ID3D12Resource> buffer;
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);

    m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        initState, nullptr, IID_PPV_ARGS(&buffer));

    return buffer;
}

UINT64 DXRRenderer::AlignUp(UINT64 value, UINT64 alignment) {
    return ( value + alignment - 1 ) & ~( alignment - 1 );
}

void DXRRenderer::WaitForGPU() {
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);

    if ( m_fence->GetCompletedValue() < m_fenceValue ) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}