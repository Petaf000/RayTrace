#include "Renderer.h"

#include "App.h"
#include "GameManager.h"

#include "Scene.h"
#include "RayTracingObject.h"
#include "Shape.h"
#include "Material.h"

#include "Box.h"
#include "Plane.h"
#include "Sphere.h"
#include "LambertMaterial.h"
#include "MetalMaterial.h"
#include "DielectricMaterial.h"
#include "EmissiveMaterial.h"

#include "CornellBoxScene.h"

ComPtr<ID3D12Resource> Renderer::CreateBuffer(UINT64 size,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initialState,
    const D3D12_HEAP_PROPERTIES& heapProps) {
	
    ComPtr<ID3D12Resource> buffer;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = size;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = flags;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&buffer)));

    return buffer;
}


Renderer::Renderer()
    : m_frameIndex(0), m_rtvDescriptorSize(0), m_fenceValue(0), m_fenceEvent(nullptr),
    m_bufferWidth(0), m_bufferHeight(0) {
}

Renderer::~Renderer() {
    Cleanup();
}

void Renderer::Init() {
    m_bufferWidth = 1920;
    m_bufferHeight = 1080;
    m_hWnd = App::GetWindowHandle();
    m_raytracing = wcscmp(App::GetCommand(), L"-raytracing") == 0;

    LoadPipeline();

    LoadAssets();

    // DXR�C���^�[�t�F�[�X�̍쐬
    CreateRaytracingInterfaces();

    // ���C�g���T�|�[�g���Ă鎞�̂�
    if ( m_device5 ) {

#ifdef _DEBUG
        {
            ComPtr<ID3D12InfoQueue> infoQueue;
            if ( SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue))) ) {
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
            }
        }
#endif


        LoadCompiledShader(L"shader/raygen.cso", m_rayGenShaderBlob);
        LoadCompiledShader(L"shader/miss.cso", m_missShaderBlob);
        LoadCompiledShader(L"shader/closesthit.cso", m_hitShaderBlob);

        CreateRaytracingRootSignature();
        CreateRaytracingDescriptorHeap();
        CreateRaytracingOutputResource();
        CreateMaterialConstantBuffers();
        CreateRaytracingPipelineState();


        Singleton<GameManager>::getInstance().OpenScene<CornellBoxScene>();

        // �V�[�����Z�b�g����Ă���ꍇ�A�A�N�Z�����[�V�����\�����쐬
        if ( Singleton<GameManager>::getInstance().GetScene() ) {
            CreateSceneAccelerationStructures();
        }
        // Must be called after CreateSceneAccelerationStructures
        CreateRaytracingShaderBindingTable();
    }
    

    // imgui�̏�����
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.FontGlobalScale = 1.5f;

        
        ImGui::StyleColorsDark();


        ImGui_ImplWin32_Init(m_hWnd);
        ImGui_ImplDX12_Init(m_device.Get(), m_frameBufferCount,
            DXGI_FORMAT_R8G8B8A8_UNORM, m_srvHeap.Get(),
            m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
            m_srvHeap->GetGPUDescriptorHandleForHeapStart());
    }
    return;
}

void Renderer::LoadCompiledShader(const wchar_t* filename, ComPtr<ID3DBlob>& shaderBlob) {
    // �t�@�C������R���p�C���ς݃V�F�[�_�[��ǂݍ���
    FILE* pFile = nullptr;
    _wfopen_s(&pFile, filename, L"rb");
    if ( !pFile ) {
        throw std::runtime_error("�V�F�[�_�[�t�@�C�����J���܂���ł����B");
    }

    // �t�@�C���T�C�Y���擾
    fseek(pFile, 0, SEEK_END);
    long fileSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    // �V�F�[�_�[�o�C�i�����i�[���邽�߂̃��������m��
    D3DCreateBlob(fileSize, &shaderBlob);

    // �t�@�C����ǂݍ���
    fread(shaderBlob->GetBufferPointer(), 1, fileSize, pFile);
    fclose(pFile);
}


void Renderer::LoadPipeline() {
    UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        if ( SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))) ) {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    // �f�o�C�X�̍쐬
    ThrowIfFailed(D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device)
    ));

    // �R�}���h�L���[�̍쐬
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // �X���b�v�`�F�[���̍쐬
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = m_frameBufferCount;
    swapChainDesc.Width = m_bufferWidth;
    swapChainDesc.Height = m_bufferHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        m_hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    // Alt+Enter �ɂ��S��ʐؑւ𖳌���
    ThrowIfFailed(factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // �����_�[�^�[�Q�b�g�r���[�p�f�X�N���v�^�q�[�v�̍쐬
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = m_frameBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));


    // �t���[���o�b�t�@�̍쐬
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for ( UINT i = 0; i < m_frameBufferCount; i++ ) {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

    return;
}

void Renderer::LoadAssets() {
    // 1. ��̃��[�g�V�O�l�`���̍쐬
    {
        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = 0;
        rootSignatureDesc.pParameters = nullptr;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // 2. �V�F�[�_�[�̃R���p�C���ƃp�C�v���C���X�e�[�g�̍쐬
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;
#if defined(_DEBUG)
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif
        const char* vertexShaderSource =
            "struct VS_INPUT { float3 pos : POSITION; float4 color : COLOR; };"
            "struct PS_INPUT { float4 pos : SV_POSITION; float4 color : COLOR; };"
            "PS_INPUT main(VS_INPUT input) {"
            "   PS_INPUT output;"
            "   output.pos = float4(input.pos, 1.0);"
            "   output.color = input.color;"
            "   return output;"
            "}";
        const char* pixelShaderSource =
            "struct PS_INPUT { float4 pos : SV_POSITION; float4 color : COLOR; };"
            "float4 main(PS_INPUT input) : SV_TARGET {"
            "   return input.color;"
            "}";

        ThrowIfFailed(D3DCompile(vertexShaderSource, std::strlen(vertexShaderSource), nullptr, nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompile(pixelShaderSource, std::strlen(pixelShaderSource), nullptr, nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }
    // 3. �R�}���h���X�g�̍쐬
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
    OutputDebugStringW(L"CommandList closed Load Asset\n");
    ThrowIfFailed(m_commandList->Close());

    // 4. **�����I�u�W�F�N�g�̍쐬�iFence�ƃC�x���g�j���ɍs��**
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if ( m_fenceEvent == nullptr ) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    // 5. ���_�o�b�t�@�̍쐬�ƃA�b�v���[�h
    {
        Vertex triangleVertices[] = {
            { {  0.0f,  0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { {  0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        };
        const UINT vertexBufferSize = sizeof(triangleVertices);

        ComPtr<ID3D12Resource> vertexBufferUpload;

        // �f�t�H���g�q�[�v�ƃA�b�v���[�h�q�[�v�̃v���p�e�B�����[�J���ϐ��Ƃ��č쐬
        CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &vertexBufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)
        ));

        CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC vertexBufferUploadDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &vertexBufferUploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBufferUpload)
        ));

        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = reinterpret_cast<BYTE*>( triangleVertices );
        vertexData.RowPitch = vertexBufferSize;
        vertexData.SlicePitch = vertexData.RowPitch;

        ThrowIfFailed(m_commandAllocator->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

        UpdateSubresources<1>(m_commandList.Get(), m_vertexBuffer.Get(), vertexBufferUpload.Get(), 0, 0, 1, &vertexData);

        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        m_commandList->ResourceBarrier(1, &transition);

        OutputDebugStringW(L"CommandList closed Load Asset\n");
        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        WaitGPU();

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }

    return;
}

void Renderer::CreateRaytracingInterfaces() {
    // DXR�T�|�[�g�̊m�F
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    HRESULT hr = m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
    if ( FAILED(hr) || options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0 ) {
		ThrowIfFailed(E_FAIL);
    }

    // ID3D12Device5�C���^�[�t�F�[�X���擾
    ThrowIfFailed(m_device.As(&m_device5));
    // ID3D12GraphicsCommandList4�C���^�[�t�F�[�X���擾
    ThrowIfFailed(m_commandList.As(&m_commandList4));

    m_shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    // �A���C�������g�ɍ��킹�� - �C����
    m_shaderTableEntrySize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    // �������A���C�������g�v�Z
    m_shaderTableEntrySize = ( m_shaderRecordSize + ( D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1 ) )
        & ~( D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1 );
}

void Renderer::CreateRaytracingRootSignature() {
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;

    // t0: �A�N�Z�����[�V�����\��
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges.push_back(srvRange);

    // u0: �o�̓e�N�X�`��
    D3D12_DESCRIPTOR_RANGE uavRange = {};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 1;
    uavRange.BaseShaderRegister = 0;
    uavRange.RegisterSpace = 0;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges.push_back(uavRange);

    // t1: ���_�o�b�t�@
    D3D12_DESCRIPTOR_RANGE vertexRange = {};
    vertexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    vertexRange.NumDescriptors = 1;
    vertexRange.BaseShaderRegister = 1;
    vertexRange.RegisterSpace = 0;
    vertexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges.push_back(vertexRange);

    // t2: �F�o�b�t�@
    D3D12_DESCRIPTOR_RANGE colorRange = {};
    colorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    colorRange.NumDescriptors = 1;
    colorRange.BaseShaderRegister = 2;
    colorRange.RegisterSpace = 0;
    colorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges.push_back(colorRange);

    // t3: �}�e���A���o�b�t�@
    D3D12_DESCRIPTOR_RANGE materialRange = {};
    materialRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    materialRange.NumDescriptors = 1;
    materialRange.BaseShaderRegister = 3; 
    materialRange.RegisterSpace = 0;
    materialRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges.push_back(materialRange);

    // �f�B�X�N���v�^�e�[�u��
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParam.DescriptorTable.NumDescriptorRanges = static_cast<UINT>( ranges.size() );
    rootParam.DescriptorTable.pDescriptorRanges = ranges.data();
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // ���[�g�V�O�l�`���̐ݒ�
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParam;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    // �V���A���C�Y���č쐬
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_dxrGlobalRootSignature)));
}

void Renderer::CreateRaytracingDescriptorHeap() {
    // �f�X�N���v�^�q�[�v�̍쐬�iTLAS, Output, Vertex buffer, Color buffer, Material buffer�j
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 5; // �ȑO��4���������A�}�e���A���o�b�t�@��ǉ�
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dxrDescriptorHeap)));

    m_dxrDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Renderer::CreateRaytracingOutputResource() {
    // ���C�g���[�V���O�o�̓��\�[�X�̍쐬
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Alignment = 0;
    resDesc.Width = m_bufferWidth;
    resDesc.Height = m_bufferHeight;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    resDesc.SampleDesc.Count = 1;
    resDesc.SampleDesc.Quality = 0;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_dxrOutput)));

    // UAV�̍쐬
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_dxrDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_dxrDescriptorSize);
    m_device->CreateUnorderedAccessView(m_dxrOutput.Get(), nullptr, &uavDesc, uavHandle);
}

void Renderer::CreateRaytracingPipelineState() {
    // �p�C�v���C����ԃI�u�W�F�N�g�̐ݒ�
    D3D12_STATE_OBJECT_DESC pipelineDesc = {};
    pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

    // DXIL���C�u�����T�u�I�u�W�F�N�g
    D3D12_DXIL_LIBRARY_DESC dxilLibDesc[3] = {};
    D3D12_EXPORT_DESC dxilExports[3] = {};

    // ���C�W�F�l���[�V�����V�F�[�_�[
    dxilLibDesc[0].DXILLibrary.pShaderBytecode = m_rayGenShaderBlob->GetBufferPointer();
    dxilLibDesc[0].DXILLibrary.BytecodeLength = m_rayGenShaderBlob->GetBufferSize();
    dxilExports[0].Name = L"RaygenShader";
    dxilExports[0].ExportToRename = nullptr;
    dxilExports[0].Flags = D3D12_EXPORT_FLAG_NONE;
    dxilLibDesc[0].NumExports = 1;
    dxilLibDesc[0].pExports = &dxilExports[0];

    // �~�X�V�F�[�_�[
    dxilLibDesc[1].DXILLibrary.pShaderBytecode = m_missShaderBlob->GetBufferPointer();
    dxilLibDesc[1].DXILLibrary.BytecodeLength = m_missShaderBlob->GetBufferSize();
    dxilExports[1].Name = L"MissShader";
    dxilExports[1].ExportToRename = nullptr;
    dxilExports[1].Flags = D3D12_EXPORT_FLAG_NONE;
    dxilLibDesc[1].NumExports = 1;
    dxilLibDesc[1].pExports = &dxilExports[1];

    // �q�b�g�V�F�[�_�[
    dxilLibDesc[2].DXILLibrary.pShaderBytecode = m_hitShaderBlob->GetBufferPointer();
    dxilLibDesc[2].DXILLibrary.BytecodeLength = m_hitShaderBlob->GetBufferSize();
    dxilExports[2].Name = L"ClosestHitShader";
    dxilExports[2].ExportToRename = nullptr;
    dxilExports[2].Flags = D3D12_EXPORT_FLAG_NONE;
    dxilLibDesc[2].NumExports = 1;
    dxilLibDesc[2].pExports = &dxilExports[2];

    // �T�u�I�u�W�F�N�g�̔z��
    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.resize(7); // DXIL���C�u����x3, �q�b�g�O���[�v, �V�F�[�_�[�ݒ�, �p�C�v���C���ݒ�, �O���[�o�����[�g�V�O�l�`��

    // DXIL���C�u�����̒ǉ��i3�j
    for ( int i = 0; i < 3; i++ ) {
        subobjects[i].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        subobjects[i].pDesc = &dxilLibDesc[i];
    }

    // �q�b�g�O���[�v
    D3D12_HIT_GROUP_DESC hitGroupDesc = {};
    hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroupDesc.ClosestHitShaderImport = L"ClosestHitShader";
    hitGroupDesc.AnyHitShaderImport = nullptr; // ����͕s�v
    hitGroupDesc.IntersectionShaderImport = nullptr; // ����͕s�v
    hitGroupDesc.HitGroupExport = L"HitGroup";

    subobjects[3].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[3].pDesc = &hitGroupDesc;

    // �V�F�[�_�[�ݒ� - �y�C���[�h�T�C�Y���g���i���ˁE���ܗp�̒ǉ����j
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = sizeof(float) * 8; // float4 color + float seed + uint recursionDepth + padding
    shaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2; // float2 barycentrics

    subobjects[4].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subobjects[4].pDesc = &shaderConfig;

    // �p�C�v���C���ݒ� - �ċA�[�x�𑝉�
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 5; // ��葽���̔��ˁE���܂��T�|�[�g

    subobjects[5].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[5].pDesc = &pipelineConfig;

    // �O���[�o�����[�g�V�O�l�`��
    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSigDesc = {};
    globalRootSigDesc.pGlobalRootSignature = m_dxrGlobalRootSignature.Get();

    subobjects[6].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subobjects[6].pDesc = &globalRootSigDesc;

    // �p�C�v���C����ԃI�u�W�F�N�g���쐬
    pipelineDesc.NumSubobjects = static_cast<UINT>( subobjects.size() );
    pipelineDesc.pSubobjects = subobjects.data();

    // ���C�g���[�V���O�p�C�v���C����Ԃ̍쐬
    ThrowIfFailed(m_device5->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&m_rtStateObject)));
    ThrowIfFailed(m_rtStateObject.As(&m_rtStateObjectProps));
}

void Renderer::CreateMaterialConstantBuffers() {
    const UINT materialCount = 64;
    UINT constantBufferSize = sizeof(MaterialConstantBuffer) * materialCount;

    // 2�̃o�b�t�@���쐬
    // 1. �A�b�v���[�h�o�b�t�@ (CPU�������ݗp)
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Width = constantBufferSize;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_materialConstantBuffer)));

    // SRV�̍쐬
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = materialCount;
    srvDesc.Buffer.StructureByteStride = sizeof(MaterialConstantBuffer);
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_dxrDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 4, m_dxrDescriptorSize);
    m_device->CreateShaderResourceView(m_materialConstantBuffer.Get(), &srvDesc, srvHandle);

    // �}�e���A���f�[�^�̃}�b�s���O
    void* mappedData;
    m_materialConstantBuffer->Map(0, nullptr, &mappedData);
    m_materialConstantBufferData = static_cast<MaterialConstantBuffer*>( mappedData );
}

void Renderer::UpdateMaterialData(UINT materialIndex, const Material* material) {
    if ( !material || materialIndex >= 64 ) {
        return;
    }

    XMFLOAT3 baseColor = material->GetColor();
    float metallic = 0.0f;
    float roughness = 0.0f;
    float ior = 1.0f;
    XMFLOAT3 emissiveColor = XMFLOAT3(0.0f, 0.0f, 0.0f);
    float emissiveIntensity = 0.0f;

    // �}�e���A���^�C�v�ɉ����ăp�����[�^��ݒ�
    MaterialType type = material->GetType();
    if ( type == MaterialType::Metal ) {
        metallic = 1.0f;
        roughness = material->GetParam();
    }
    else if ( type == MaterialType::Dielectric ) {
        ior = material->GetParam();
    }
    else if ( type == MaterialType::Emissive ) {
        emissiveColor = baseColor;
        emissiveIntensity = material->GetParam();
        baseColor = XMFLOAT3(0.0f, 0.0f, 0.0f); // �����̂̊�{�F�͍���
    }

    // �萔�o�b�t�@���X�V
    m_materialConstantBufferData[materialIndex].baseColor = XMFLOAT4(baseColor.x, baseColor.y, baseColor.z, 1.0f);
    m_materialConstantBufferData[materialIndex].emissiveColor = XMFLOAT4(
        emissiveColor.x * emissiveIntensity,
        emissiveColor.y * emissiveIntensity,
        emissiveColor.z * emissiveIntensity,
        1.0f);
    m_materialConstantBufferData[materialIndex].metallic = metallic;
    m_materialConstantBufferData[materialIndex].roughness = roughness;
    m_materialConstantBufferData[materialIndex].ior = ior;
}

void Renderer::CreateSceneAccelerationStructures() {
    // �����̃A�N�Z�����[�V�����\�����N���A
    /*m_bottomLevelASs.clear();
    m_instanceDescs.clear();*/

    // �����̃R�}���h���X�g�����s���łȂ����Ƃ��m�F
    if ( m_commandList ) {
        // �O�̂��ߌ��݂̃R�}���h���X�g�����i���łɕ����Ă���ꍇ�͉e���Ȃ��j
        OutputDebugStringW(L"CommandList closed Create AS\n");

        HRESULT hrClose = m_commandList->Close();
        if ( SUCCEEDED(hrClose) ) { // �R�}���h���X�g���J���Ă����ꍇ
            ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
            m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        }

        // GPU�̏���������҂�
        WaitGPU();
    }

    Scene* scene;
    if ( !( scene = Singleton<GameManager>::getInstance().GetScene().get() ) ) {
        return;
    }

    // �V�[�����烌�C�g���[�V���O�I�u�W�F�N�g���擾
    auto rayTracingObjs = scene->GetGameObjects<RayTracingObject>();
    if ( rayTracingObjs.empty() ) {
        return;
    }

    // �R�}���h���X�g�̃��Z�b�g
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    // �q�[�v�v���p�e�B�������Œ�`
    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // **�d�v�ȏC��**: �ꎞ���\�[�X��ێ����邽�߂̃R���e�i���쐬
    std::vector<ComPtr<ID3D12Resource>> tempResources;

    // �e�I�u�W�F�N�g��BLAS���\�z
    for ( size_t i = 0; i < rayTracingObjs.size(); i++ ) {
        auto* obj = rayTracingObjs[i];
        if ( !obj || !obj->GetShape() ) {
            continue;
        }

        // �W�I���g���L�q���擾
        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = obj->GetShape()->GetGeometryDesc();

        // BLAS���͂�ݒ�
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
        blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        blasInputs.NumDescs = 1;
        blasInputs.pGeometryDescs = &geometryDesc;
        blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

        // �v���r���h�����擾
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo = {};
        m_device5->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasInfo);

        // �X�N���b�`�o�b�t�@���쐬
        ComPtr<ID3D12Resource> scratchBuffer;
        D3D12_RESOURCE_DESC scratchDesc = CD3DX12_RESOURCE_DESC::Buffer(
            blasInfo.ScratchDataSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &scratchDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(&scratchBuffer)));

        // �ꎞ���\�[�X�Ƃ��Ēǉ�
        tempResources.push_back(scratchBuffer);

        // BLAS�p�̃o�b�t�@���쐬
        ComPtr<ID3D12Resource> blasBuffer;
        D3D12_RESOURCE_DESC blasDesc = CD3DX12_RESOURCE_DESC::Buffer(
            blasInfo.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(m_device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &blasDesc,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            nullptr,
            IID_PPV_ARGS(&blasBuffer)));

        // BLAS�\�z�R�}���h�̔��s
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc = {};
        blasBuildDesc.Inputs = blasInputs;
        blasBuildDesc.DestAccelerationStructureData = blasBuffer->GetGPUVirtualAddress();
        blasBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();

        if ( m_commandList4 ) {
            m_commandList4->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);
        }

        // UAV�o���A
        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = blasBuffer.Get();
        m_commandList->ResourceBarrier(1, &uavBarrier);

        // BLAS��ۑ�
        m_bottomLevelASs.push_back(blasBuffer);

        // �C���X�^���X�L�q�q���쐬
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};

        // �I�u�W�F�N�g�̃g�����X�t�H�[�����s��ɕϊ�
        XMFLOAT3 position = obj->GetPosition();
        XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);

        // �s���3x4�`���ɕϊ����ăC���X�^���X�L�q�q�ɃR�s�[
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>( instanceDesc.Transform ), translationMatrix);

        instanceDesc.InstanceID = static_cast<UINT>( i );  // �C���X�^���XID
        instanceDesc.InstanceMask = 0xFF;                // ���ׂẴ��C�ƌ���
        instanceDesc.InstanceContributionToHitGroupIndex = 0; // �q�b�g�O���[�v�C���f�b�N�X
        instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc.AccelerationStructure = blasBuffer->GetGPUVirtualAddress();

        m_instanceDescs.push_back(instanceDesc);

        // �}�e���A���f�[�^���X�V
        if ( obj->GetShape()->GetMaterial() ) {
            UpdateMaterialData(static_cast<UINT>( i ), obj->GetShape()->GetMaterial());
        }
    }

    // �C���X�^���X�o�b�t�@�̍쐬
    UINT instanceDescSize = static_cast<UINT>( m_instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC) );
    if ( instanceDescSize == 0 ) {
        return; // �C���X�^���X���Ȃ��ꍇ�͏I��
    }

    // �A�b�v���[�h�q�[�v���쐬
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    ComPtr<ID3D12Resource> instanceDescsBuffer;
    D3D12_RESOURCE_DESC instanceDescBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(instanceDescSize);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &instanceDescBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&instanceDescsBuffer)));

    // �ꎞ���\�[�X�Ƃ��Ēǉ�
    tempResources.push_back(instanceDescsBuffer);

    // �C���X�^���X�L�q�q���o�b�t�@�ɃR�s�[
    void* mappedData;
    ThrowIfFailed(instanceDescsBuffer->Map(0, nullptr, &mappedData));
    memcpy(mappedData, m_instanceDescs.data(), instanceDescSize);
    instanceDescsBuffer->Unmap(0, nullptr);

    // TLAS���͂̐ݒ�
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlasInputs.NumDescs = static_cast<UINT>( m_instanceDescs.size() );
    tlasInputs.InstanceDescs = instanceDescsBuffer->GetGPUVirtualAddress();
    tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    // TLAS�p�̃v���r���h�����擾
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo = {};
    m_device5->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasInfo);

    // TLAS�X�N���b�`�o�b�t�@�̍쐬
    ComPtr<ID3D12Resource> tlasScratchBuffer;
    D3D12_RESOURCE_DESC tlasScratchDesc = CD3DX12_RESOURCE_DESC::Buffer(
        tlasInfo.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &tlasScratchDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&tlasScratchBuffer)));

    // �ꎞ���\�[�X�Ƃ��Ēǉ�
    tempResources.push_back(tlasScratchBuffer);

    // TLAS�o�b�t�@�̍쐬
    D3D12_RESOURCE_DESC tlasDesc = CD3DX12_RESOURCE_DESC::Buffer(
        tlasInfo.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &tlasDesc,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nullptr,
        IID_PPV_ARGS(&m_topLevelAS)));

    // TLAS�\�z�R�}���h�̔��s
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc = {};
    tlasBuildDesc.Inputs = tlasInputs;
    tlasBuildDesc.DestAccelerationStructureData = m_topLevelAS->GetGPUVirtualAddress();
    tlasBuildDesc.ScratchAccelerationStructureData = tlasScratchBuffer->GetGPUVirtualAddress();

    if ( m_commandList4 ) {
        m_commandList4->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);
    }

    // UAV�o���A
    D3D12_RESOURCE_BARRIER tlasuavBarrier = {};
    tlasuavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    tlasuavBarrier.UAV.pResource = m_topLevelAS.Get();
    m_commandList->ResourceBarrier(1, &tlasuavBarrier);

    // TLAS��SRV���쐬
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_topLevelAS->GetGPUVirtualAddress();

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_dxrDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    m_device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

    // �R�}���h���X�g�����s
    OutputDebugStringW(L"CommandList closed Create AS\n");
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // ������ҋ@
    WaitGPU();

    // **���̎��_�ŏ��߂Ĉꎞ���\�[�X�̉�������S**
    // tempResources�̓X�R�[�v�𔲂���Ǝ����I�ɉ�������
}

void Renderer::CreateRaytracingShaderBindingTable() {
    // �V�F�[�_�[���ʎq�̃T�C�Y�ƕK�v�ȃA���C�������g
    UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    UINT shaderRecordAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    UINT tableAlignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

    // �V�F�[�_�[���R�[�h�T�C�Y - ���[�J�����[�g�V�O�l�`���Ȃ��̏ꍇ�͎��ʎq�̂�
    UINT shaderRecordSize = shaderIdentifierSize;
    // �A���C�����g���l��
    UINT alignedShaderRecordSize = ( shaderRecordSize + ( shaderRecordAlignment - 1 ) ) & ~( shaderRecordAlignment - 1 );

    // �e�e�[�u���̃T�C�Y���v�Z�i64�o�C�g�A���C�����g�l���j
    UINT rayGenSectionSize = ( alignedShaderRecordSize + ( tableAlignment - 1 ) ) & ~( tableAlignment - 1 );
    UINT missSectionSize = ( alignedShaderRecordSize + ( tableAlignment - 1 ) ) & ~( tableAlignment - 1 );
    UINT hitGroupSectionSize = ( alignedShaderRecordSize + ( tableAlignment - 1 ) ) & ~( tableAlignment - 1 );

    // �V�F�[�_�[�e�[�u���S�̂̃T�C�Y
    UINT shaderTableSize = rayGenSectionSize + missSectionSize + hitGroupSectionSize;

    // �V�F�[�_�[�e�[�u���o�b�t�@���쐬
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = shaderTableSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_shaderTable)));

    // �V�F�[�_�[���ʎq���擾
    void* rayGenShaderID = m_rtStateObjectProps->GetShaderIdentifier(L"RaygenShader");
    void* missShaderID = m_rtStateObjectProps->GetShaderIdentifier(L"MissShader");
    void* hitGroupShaderID = m_rtStateObjectProps->GetShaderIdentifier(L"HitGroup");

    // �V�F�[�_�[�e�[�u���ɏ�������
    uint8_t* pData;
    ThrowIfFailed(m_shaderTable->Map(0, nullptr, reinterpret_cast<void**>( &pData )));
    memset(pData, 0, shaderTableSize); // �S�̂��[���ŏ�����

    // �e�V�F�[�_�[���ʎq���R�s�[
    memcpy(pData, rayGenShaderID, shaderIdentifierSize);
    memcpy(pData + rayGenSectionSize, missShaderID, shaderIdentifierSize);
    memcpy(pData + rayGenSectionSize + missSectionSize, hitGroupShaderID, shaderIdentifierSize);

    m_shaderTable->Unmap(0, nullptr);

    // �V�F�[�_�[�e�[�u���̊e�Z�N�V������GPU�A�h���X��ۑ�
    m_rayGenShaderTableGpuAddr = m_shaderTable->GetGPUVirtualAddress();
    m_missShaderTableGpuAddr = m_shaderTable->GetGPUVirtualAddress() + rayGenSectionSize;
    m_hitGroupShaderTableGpuAddr = m_shaderTable->GetGPUVirtualAddress() + rayGenSectionSize + missSectionSize;

    // �V�F�[�_�[�e�[�u���̊e�Z�N�V�����̃T�C�Y��ۑ�
    m_rayGenSectionSize = rayGenSectionSize;
    m_missSectionSize = missSectionSize;
    m_hitGroupSectionSize = hitGroupSectionSize;
    m_shaderRecordSize = alignedShaderRecordSize;
}

void Renderer::Update() {
    // �X�V�����i�K�v�ɉ����āj
}

void Renderer::Render() {
	WaitGPU();  // GPU�̏�������������܂őҋ@
    // �R�}���h���X�g�̃��Z�b�g
    ThrowIfFailed(m_commandAllocator->Reset());

    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));  // �p�C�v���C����nullptr�ɐݒ�


    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();


    // ImGui�̃t���[���J�n
    {
        // Start the Dear ImGui frame

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame((float)App::GetWindowSize().Width / m_bufferWidth, (float)App::GetWindowSize().Height / m_bufferHeight
            , (float)m_bufferWidth, (float)m_bufferHeight);
        ImGui::NewFrame();
    }

    // �o�b�N�o�b�t�@�������_�[�^�[�Q�b�g�ɐݒ�
    auto subBuf = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &subBuf);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // ��ʃN���A
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    if ( m_raytracing&& m_device5 ) {
        // �f�X�N���v�^�q�[�v�̐ݒ�
        ID3D12DescriptorHeap* heaps[] = { m_dxrDescriptorHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        // �p�C�v���C���X�e�[�g��ݒ�
        m_commandList4->SetPipelineState1(m_rtStateObject.Get());

        // ���[�g�V�O�l�`����ݒ�
        m_commandList->SetComputeRootSignature(m_dxrGlobalRootSignature.Get());

        // �f�X�N���v�^�e�[�u����ݒ�
        m_commandList->SetComputeRootDescriptorTable(0, m_dxrDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

        // ���C�f�B�X�p�b�`�L�q
        D3D12_DISPATCH_RAYS_DESC rayDesc = {};

        // ���C�W�F�l���[�V�����V�F�[�_�[���R�[�h
        rayDesc.RayGenerationShaderRecord.StartAddress = m_rayGenShaderTableGpuAddr;
        rayDesc.RayGenerationShaderRecord.SizeInBytes = m_shaderRecordSize;

        // �~�X�V�F�[�_�[�e�[�u��
        rayDesc.MissShaderTable.StartAddress = m_missShaderTableGpuAddr;
        rayDesc.MissShaderTable.SizeInBytes = m_shaderRecordSize;
        rayDesc.MissShaderTable.StrideInBytes = m_shaderRecordSize;

        // �q�b�g�O���[�v�e�[�u��
        rayDesc.HitGroupTable.StartAddress = m_hitGroupShaderTableGpuAddr;
        rayDesc.HitGroupTable.SizeInBytes = m_shaderRecordSize;
        rayDesc.HitGroupTable.StrideInBytes = m_shaderRecordSize;

        // ���C�g���[�V���O�𑜓x
        rayDesc.Width = m_bufferWidth;
        rayDesc.Height = m_bufferHeight;
        rayDesc.Depth = 1;

        // ���C�𔭎�
        m_commandList4->DispatchRays(&rayDesc);

        // ���C�g���[�V���O�o�͂��o�b�N�o�b�t�@�ɃR�s�[
        D3D12_RESOURCE_BARRIER barriers[2];
        barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_dxrOutput.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST);

        m_commandList->ResourceBarrier(2, barriers);

        m_commandList->CopyResource(m_renderTargets[m_frameIndex].Get(), m_dxrOutput.Get());

        barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_dxrOutput.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        m_commandList->ResourceBarrier(2, barriers);
    }
    else {
        m_commandList->SetPipelineState(m_pipelineState.Get());
        // �f�X�N���v�^�q�[�v�̐ݒ�
        {
            ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
            m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        }

        // ���[�g�V�O�l�`���̐ݒ�
        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

        // �r���[�|�[�g�E�V�U�[��`�̐ݒ�
        CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>( m_bufferWidth ), static_cast<float>( m_bufferHeight ));
        m_commandList->RSSetViewports(1, &viewport);
        CD3DX12_RECT scissorRect(0, 0, static_cast<LONG>( m_bufferWidth ), static_cast<LONG>( m_bufferHeight ));
        m_commandList->RSSetScissorRects(1, &scissorRect);

        // �C�ӂ�ImGui�֐��̌Ăяo��
    // ���L�ł�"Hello, world!"�Ƃ����^�C�g���̃E�B���h�E��\������
        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::End();

        // �O�p�`�`��
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        m_commandList->DrawInstanced(3, 1, 0, 0);
    }

    // ���C�g���[�V���O���[�h�̏ꍇ�AImGui�`��̂��߂�srv�q�[�v�ɐ؂�ւ���
    if ( m_raytracing && m_device5 ) {
        ID3D12DescriptorHeap* imguiHeaps[] = { m_srvHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(imguiHeaps), imguiHeaps);
    }

    // ImGui UI
    ImGui::Begin("Renderer Settings");
    bool currentMode = m_raytracing;
    if ( ImGui::Checkbox("Use Raytracing", &currentMode) ) {
        if ( m_device5 ) {
            m_raytracing = currentMode;
        }
        else if ( currentMode ) {
            m_raytracing = false;
        }
    }

    if ( !m_device5 ) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "DXR is not supported on this device");
    }
    ImGui::End();

    ImGui::Render();
    
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

    subBuf = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    m_commandList->ResourceBarrier(1, &subBuf);

    OutputDebugStringW(L"CommandList closed Draw\n");
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_swapChain->Present(1, 0);

}

void Renderer::Cleanup() {
    WaitGPU();
    CloseHandle(m_fenceEvent);

    // DXR���\�[�X�̃N���[���A�b�v
    m_device5 = nullptr;
    m_commandList4 = nullptr;
    m_rtStateObject = nullptr;
    m_rtStateObjectProps = nullptr;
    m_dxrGlobalRootSignature = nullptr;
    m_topLevelAS = nullptr;
    m_shaderTable = nullptr;
    m_dxrOutput = nullptr;
    m_dxrDescriptorHeap = nullptr;
    m_vertexBuffer2 = nullptr;
    m_colorBuffer = nullptr;

    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
