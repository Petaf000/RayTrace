#include "Renderer.h"

#include "App.h"
#include "GameManager.h"

#include "Scene.h"

#include "DXRRenderer.h"
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

    LoadPipeline();

    LoadAssets();


#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue;
    if ( SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue))) ) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    }
    ComPtr<ID3D12Debug> debug;
    D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
    debug->EnableDebugLayer();
#endif // _DEBUG
    // ★ ImGuiの初期化（Rendererで一元管理） ★
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.FontGlobalScale = 1.5f;

        ImGui::StyleColorsLight();

        ImGui_ImplWin32_Init(m_hWnd);

        // ★ RendererのSRVヒープを使用 ★
        ImGui_ImplDX12_Init(m_device.Get(), 2,
            DXGI_FORMAT_R8G8B8A8_UNORM, m_srvHeap.Get(),
            m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
            m_srvHeap->GetGPUDescriptorHandleForHeapStart());

        OutputDebugStringA("ImGui initialized with Renderer's SRV heap\n");
    }
    return;
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

    // 最高性能のアダプターを選択
    ComPtr<IDXGIAdapter1> bestAdapter = GetBestAdapter(factory.Get());

    // 選択したアダプターでデバイス作成
    ThrowIfFailed(D3D12CreateDevice(
        bestAdapter.Get(),  // 最高性能アダプター使用
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device)
    ));

    // コマンドキューの作成
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // スワップチェーンの作成
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

    // Alt+Enter による全画面切替を無効化
    ThrowIfFailed(factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // レンダーターゲットビュー用デスクリプタヒープの作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = m_frameBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 128;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));


    // フレームバッファの作成
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
    // 1. 空のルートシグネチャの作成
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

    // 2. シェーダーのコンパイルとパイプラインステートの作成
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
    // 3. コマンドリストの作成
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
    OutputDebugStringW(L"CommandList closed Load Asset\n");
    ThrowIfFailed(m_commandList->Close());

    // 4. **同期オブジェクトの作成（Fenceとイベント）を先に行う**
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if ( m_fenceEvent == nullptr ) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    // 5. 頂点バッファの作成とアップロード
    {
        Vertex triangleVertices[] = {
            { {  0.0f,  0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { {  0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        };
        const UINT vertexBufferSize = sizeof(triangleVertices);

        ComPtr<ID3D12Resource> vertexBufferUpload;

        // デフォルトヒープとアップロードヒープのプロパティをローカル変数として作成
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

		ResetCommandList();

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

ComPtr<IDXGIAdapter1> Renderer::GetBestAdapter(IDXGIFactory4* factory) {
    ComPtr<IDXGIAdapter1> bestAdapter;
    ComPtr<IDXGIAdapter1> adapter;
    SIZE_T maxVideoMemory = 0;
    char debugMsg[512];

    sprintf_s(debugMsg, "=== DXR GPU Selection ===\n");
    OutputDebugStringA(debugMsg);

    for ( UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++ ) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) {
            continue;
        }

        // D3D12デバイス作成テスト
        ComPtr<ID3D12Device5> testDevice;
        if ( SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice))) ) {

            char gpuName[256];
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, gpuName, sizeof(gpuName), nullptr, nullptr);
            SIZE_T vramMB = desc.DedicatedVideoMemory / ( 1024 * 1024 );

            // DXR対応チェック
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 dxrSupport = {};
            bool supportsDXR = false;
            if ( SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &dxrSupport, sizeof(dxrSupport))) ) {
                supportsDXR = ( dxrSupport.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0 );
            }

            sprintf_s(debugMsg, "GPU %d: %s\n", i, gpuName);
            OutputDebugStringA(debugMsg);
            sprintf_s(debugMsg, "  VRAM: %zu MB, DXR: %s\n", vramMB, supportsDXR ? "Yes" : "No");
            OutputDebugStringA(debugMsg);

            // 専用ビデオメモリが最大のものを選択
            if ( desc.DedicatedVideoMemory > maxVideoMemory ) {
                maxVideoMemory = desc.DedicatedVideoMemory;
                bestAdapter = adapter;
            }
        }
    }

    if ( bestAdapter ) {
        DXGI_ADAPTER_DESC1 selectedDesc;
        bestAdapter->GetDesc1(&selectedDesc);

        char selectedGpuName[256];
        WideCharToMultiByte(CP_UTF8, 0, selectedDesc.Description, -1, selectedGpuName, sizeof(selectedGpuName), nullptr, nullptr);
        SIZE_T selectedVramMB = selectedDesc.DedicatedVideoMemory / ( 1024 * 1024 );

        // 選択されたGPUのDXR情報も取得
        ComPtr<ID3D12Device5> finalDevice;
        bool finalDXRSupport = false;
        if ( SUCCEEDED(D3D12CreateDevice(bestAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&finalDevice))) ) {
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 dxrSupport = {};
            if ( SUCCEEDED(finalDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &dxrSupport, sizeof(dxrSupport))) ) {
                finalDXRSupport = ( dxrSupport.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0 );
            }
        }

        sprintf_s(debugMsg, "=== SELECTED GPU ===\n");
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "Name: %s\n", selectedGpuName);
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "VRAM: %zu MB\n", selectedVramMB);
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "DXR Support: %s\n", finalDXRSupport ? "Yes" : "No");
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "===================\n");
        OutputDebugStringA(debugMsg);
    }
    else {
        sprintf_s(debugMsg, "ERROR: No compatible GPU found!\n");
        OutputDebugStringA(debugMsg);
    }

    return bestAdapter;
}

void Renderer::Update() {
    // 更新処理（必要に応じて）
}

void Renderer::Render() {
    m_commandList->SetPipelineState(m_pipelineState.Get());
    // デスクリプタヒープの設定
    {
        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    }

    // ルートシグネチャの設定
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // 三角形描画
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::InitFrame() {
    WaitGPU();  // GPUの処理が完了するまで待機

    // コマンドリストのリセット
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    OutputDebugStringA("InitFrameForDXR: Setting up for DXR rendering\n");

    // ImGuiのフレーム開始（DXR用）
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame((float)App::GetWindowSize().Width / m_bufferWidth,
            (float)App::GetWindowSize().Height / m_bufferHeight,
            (float)m_bufferWidth, (float)m_bufferHeight);
        ImGui::NewFrame();
    }

    // ★ DXR用：バックバッファの状態遷移はDXRRenderer側で行う ★
    // ここではバックバッファの状態遷移を行わない（PRESENT状態のまま）

    OutputDebugStringA("InitFrameForDXR completed\n");

    //// バックバッファをレンダーターゲットに設定
    //auto subBuf = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
    //    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    //m_commandList->ResourceBarrier(1, &subBuf);

    //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    //m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    //// 画面クリア
    //const float clearColor[] = { 1.0f, 0.2f, 0.4f, 1.0f };
    //m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    //// ビューポート・シザー矩形の設定
    //CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>( Singleton<DXRRenderer>::getInstance().GetWidth() ), static_cast<float>( Singleton<DXRRenderer>::getInstance().GetHeight() ));
    //m_commandList->RSSetViewports(1, &viewport);
    //CD3DX12_RECT scissorRect(0, 0, static_cast<LONG>( Singleton<DXRRenderer>::getInstance().GetWidth() ), static_cast<LONG>( Singleton<DXRRenderer>::getInstance().GetHeight() ));
    //m_commandList->RSSetScissorRects(1, &scissorRect);
}

void Renderer::InitFrameForDXR() {
    WaitGPU();  // GPUの処理が完了するまで待機

    // コマンドリストのリセット
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // ImGuiのフレーム開始（DXR用）
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame((float)App::GetWindowSize().Width / m_bufferWidth,
            (float)App::GetWindowSize().Height / m_bufferHeight,
            (float)m_bufferWidth, (float)m_bufferHeight);
        ImGui::NewFrame();
    }
}

void Renderer::EndFrame() {
    // ★ 削除：この時点でバックバッファは DXRRenderer::Render() によって
    //         既に D3D12_RESOURCE_STATE_RENDER_TARGET 状態になっているはずなので、
    //         ここでの遷移は不要かつ間違いです。
    /* auto transitionToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    m_commandList->ResourceBarrier(1, &transitionToRT);
    */

    // ★ ビューポート・シザー矩形の設定（全画面） ★
    CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>( m_bufferWidth ), static_cast<float>( m_bufferHeight ));
    m_commandList->RSSetViewports(1, &viewport);
    CD3DX12_RECT scissorRect(0, 0, static_cast<LONG>( m_bufferWidth ), static_cast<LONG>( m_bufferHeight ));
    m_commandList->RSSetScissorRects(1, &scissorRect);

    // レンダーターゲットを設定（ImGui描画用）
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // ★ ImGui用のディスクリプタヒープを設定 ★
    // 注意：DXRRendererでデバッグビューを作成したヒープと同じものを設定してください
    ID3D12DescriptorHeap* imguiHeaps[] = { m_srvHeap.Get() }; // m_srvHeapは正しいですか？ DXRRendererのm_imguiDescriptorHeapかもしれません
    m_commandList->SetDescriptorHeaps(_countof(imguiHeaps), imguiHeaps);

    // ImGui描画
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

    // ★★★ これが唯一必要な「Presentバリア」です ★★★
    // バックバッファをPresent状態に遷移させて、画面に表示する準備をします
    auto transitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_commandList->ResourceBarrier(1, &transitionToPresent);

    // コマンドリスト実行
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_swapChain->Present(1, 0);
	WaitGPU();
}

void Renderer::Cleanup() {
    WaitGPU();
    CloseHandle(m_fenceEvent);

    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
