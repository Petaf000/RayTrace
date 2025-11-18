#include "DXRRenderer.h"
#include "Renderer.h"
#include "GameManager.h"
#include "DXRScene.h"

void DXRRenderer::Init(Renderer* renderer) {
    HRESULT hr = renderer->GetDevice()->QueryInterface(IID_PPV_ARGS(&m_device));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create DXR Device");
    }

    m_commandQueue = renderer->GetCommandQueue();

    hr = renderer->GetCommandList()->QueryInterface(IID_PPV_ARGS(&m_commandList));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create DXR CommandList");
    }

    m_width = renderer->GetBufferWidth();
    m_height = renderer->GetBufferHeight();

    char debugMsg[256];
    sprintf_s(debugMsg, "=== DXR Init with Denoiser ===\n");
    OutputDebugStringA(debugMsg);
    sprintf_s(debugMsg, "Setting DXR size to match renderer: %ux%u\n", m_width, m_height);
    OutputDebugStringA(debugMsg);

    InitializeDXR(m_device.Get());
    CreateRootSignature();
    CreateRaytracingPipelineStateObject();

    CreateAccelerationStructures();

    CreateOutputResource();
    
    CreateReSTIRResources();

    CreateShaderTables();

    CreateDenoiserResources();
    CreateDenoiserPipeline();

    D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
    imguiHeapDesc.NumDescriptors = 7;
    imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> sharedSrvHeap = renderer->GetSRVHeap();
    UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    const int dxrDebugViewOffset = 1;

    m_debugSrvHeapStart_CPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(sharedSrvHeap->GetCPUDescriptorHandleForHeapStart());
    m_debugSrvHeapStart_CPU.Offset(dxrDebugViewOffset, descriptorSize);

    m_debugSrvHeapStart_GPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(sharedSrvHeap->GetGPUDescriptorHandleForHeapStart());
    m_debugSrvHeapStart_GPU.Offset(dxrDebugViewOffset, descriptorSize);

    CreateDebugBufferViews();

    OutputDebugStringA("DXR initialization completed\n");
}

void DXRRenderer::UnInit() {
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
    
    m_lightBuffer.Reset();
    m_lightData.clear();
    m_numLights = 0;

    m_denoiserPSO.Reset();
    m_denoiserRootSignature.Reset();
    m_denoiserConstants.Reset();
    m_denoiserDescriptorHeap.Reset();
    m_albedoBuffer.Reset();
    m_normalBuffer.Reset();
    m_depthBuffer.Reset();
    m_denoisedOutput.Reset();
}

void DXRRenderer::Render() {
    static int frameCount = 0;
    frameCount++;


    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );

    if ( !dxrScene ) {
        return;
    }

    UpdateCamera();

    if (!m_temporalAccumulationInitialized) {
        
        CD3DX12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::UAV(m_accumulationBuffer.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(m_prevFrameDataBuffer.Get())
        };
        m_commandList->ResourceBarrier(2, barriers);
        
        m_temporalAccumulationInitialized = true;
        
        char debugMsg[256];
        sprintf_s(debugMsg, "Temporal Accumulation buffers initialized (auto-clear on first frame)\n");
        OutputDebugStringA(debugMsg);
    }
    
    /*if (!m_restirInitialized) {
        InitializeReSTIRBuffers();
        
        char debugMsg[256];
        sprintf_s(debugMsg, "ReSTIR DI buffers initialized\n");
        OutputDebugStringA(debugMsg);
    }*/

    auto& renderer = Singleton<Renderer>::getInstance();
    ComPtr<ID3D12Resource> currentBackBuffer = renderer.GetBackBuffer(renderer.GetCurrentFrameIndex());
    if ( !currentBackBuffer ) {
        return;
    }

    D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};

    raytraceDesc.RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = s_shaderTableEntrySize;

    raytraceDesc.MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
    raytraceDesc.MissShaderTable.SizeInBytes = s_shaderTableEntrySize;
    raytraceDesc.MissShaderTable.StrideInBytes = s_shaderTableEntrySize;

    raytraceDesc.HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
    raytraceDesc.HitGroupTable.SizeInBytes = s_hitGroupEntrySize * 4;
    raytraceDesc.HitGroupTable.StrideInBytes = s_hitGroupEntrySize;

    raytraceDesc.Width = m_width;
    raytraceDesc.Height = m_height;
    raytraceDesc.Depth = 1;

    m_commandList->SetComputeRootSignature(m_globalRootSignature.Get());

    ID3D12DescriptorHeap* dxrHeaps[] = { m_descriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, dxrHeaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE tableBaseHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->SetComputeRootDescriptorTable(0, tableBaseHandle);

    m_commandList->SetComputeRootShaderResourceView(1, m_topLevelAS->GetGPUVirtualAddress());

    m_commandList->SetComputeRootConstantBufferView(2, m_sceneConstantBuffer->GetGPUVirtualAddress());

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvTableHandle = tableBaseHandle;
    srvTableHandle.Offset(9, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_commandList->SetComputeRootDescriptorTable(3, srvTableHandle);
    m_commandList->SetPipelineState1(m_rtStateObject.Get());
    m_commandList->DispatchRays(&raytraceDesc);

    std::vector<CD3DX12_RESOURCE_BARRIER> uavBarriers = {
        CD3DX12_RESOURCE_BARRIER::UAV(m_raytracingOutput.Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(m_albedoBuffer.Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(m_normalBuffer.Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(m_depthBuffer.Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(m_currentReservoirs.Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(m_previousReservoirs.Get())
    };
    m_commandList->ResourceBarrier(static_cast<UINT>( uavBarriers.size() ), uavBarriers.data());
    
    std::vector<CD3DX12_RESOURCE_BARRIER> copyBarriers = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_currentReservoirs.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_previousReservoirs.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_DEST
        )
    };
    m_commandList->ResourceBarrier(static_cast<UINT>(copyBarriers.size()), copyBarriers.data());
    
    m_commandList->CopyResource(m_previousReservoirs.Get(), m_currentReservoirs.Get());
    
    std::vector<CD3DX12_RESOURCE_BARRIER> restoreBarriers = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_currentReservoirs.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_previousReservoirs.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        )
    };
    m_commandList->ResourceBarrier(static_cast<UINT>(restoreBarriers.size()), restoreBarriers.data());

    if ( m_denoiserEnabled ) {

        std::vector<CD3DX12_RESOURCE_BARRIER> preCopyBarriers = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_denoisedOutput.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer.Get(),
                D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
        };
        m_commandList->ResourceBarrier(static_cast<UINT>( preCopyBarriers.size() ), preCopyBarriers.data());

        m_commandList->CopyResource(currentBackBuffer.Get(), m_denoisedOutput.Get());

        std::vector<CD3DX12_RESOURCE_BARRIER> postCopyBarriers = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_denoisedOutput.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
        m_commandList->ResourceBarrier(static_cast<UINT>( postCopyBarriers.size() ), postCopyBarriers.data());
    }
    else {
        ID3D12Resource* finalResult = m_raytracingOutput.Get();

        std::vector<CD3DX12_RESOURCE_BARRIER> preCopyBarriers = {
            CD3DX12_RESOURCE_BARRIER::Transition(finalResult,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer.Get(),
                D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
        };
        m_commandList->ResourceBarrier(static_cast<UINT>( preCopyBarriers.size() ), preCopyBarriers.data());

        m_commandList->CopyResource(currentBackBuffer.Get(), finalResult);

        std::vector<CD3DX12_RESOURCE_BARRIER> postCopyBarriers = {
            CD3DX12_RESOURCE_BARRIER::Transition(finalResult,
                D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET)
        };
        m_commandList->ResourceBarrier(static_cast<UINT>( postCopyBarriers.size() ), postCopyBarriers.data());
    }
}

void DXRRenderer::RenderDXRIMGUI() {

    std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_accumulationBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_prevFrameDataBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_albedoBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_normalBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_depthBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    if ( m_denoiserEnabled ) {
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_denoisedOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }

    if ( !barriers.empty() ) {
        m_commandList->ResourceBarrier(static_cast<UINT>( barriers.size() ), barriers.data());
    }
    /*
    ImGui::Begin("Denoiser Parameters");

    static float colorSigma = 0.45f;
    static float normalSigma = 8.0f;
    static float depthSigma = 0.3f;
    static int iterations = 5;
    static bool denoiserEnabled = true;

    if ( ImGui::Checkbox("Enable Denoiser", &denoiserEnabled) ) {
        SetDenoiserEnabled(denoiserEnabled);
    }

    ImGui::Separator();

    bool parametersChanged = false;

    ImGui::Text("Color Sigma (Color difference sensitivity)");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Lower = more strict, Higher = more tolerant");
    if ( ImGui::SliderFloat("##ColorSigma", &colorSigma, 0.05f, 1.0f, "%.3f") ) {
        parametersChanged = true;
    }

    ImGui::SameLine();
    if ( ImGui::SmallButton("0.2") ) { colorSigma = 0.2f; parametersChanged = true; }
    ImGui::SameLine();
    if ( ImGui::SmallButton("0.45") ) { colorSigma = 0.45f; parametersChanged = true; }
    ImGui::SameLine();
    if ( ImGui::SmallButton("0.8") ) { colorSigma = 0.8f; parametersChanged = true; }

    ImGui::Spacing();

    ImGui::Text("Normal Sigma (Surface normal similarity)");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Lower = more strict, Higher = more tolerant");
    if ( ImGui::SliderFloat("##NormalSigma", &normalSigma, 1.0f, 64.0f, "%.1f") ) {
        parametersChanged = true;
    }

    ImGui::SameLine();
    if ( ImGui::SmallButton("4.0") ) { normalSigma = 4.0f; parametersChanged = true; }
    ImGui::SameLine();
    if ( ImGui::SmallButton("8.0") ) { normalSigma = 8.0f; parametersChanged = true; }
    ImGui::SameLine();
    if ( ImGui::SmallButton("16.0") ) { normalSigma = 16.0f; parametersChanged = true; }

    ImGui::Spacing();

    ImGui::Text("Depth Sigma (Depth difference sensitivity)");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Lower = more strict, Higher = more tolerant");
    if ( ImGui::SliderFloat("##DepthSigma", &depthSigma, 0.01f, 1.0f, "%.3f") ) {
        parametersChanged = true;
    }

    ImGui::SameLine();
    if ( ImGui::SmallButton("0.1") ) { depthSigma = 0.1f; parametersChanged = true; }
    ImGui::SameLine();
    if ( ImGui::SmallButton("0.3") ) { depthSigma = 0.3f; parametersChanged = true; }
    ImGui::SameLine();
    if ( ImGui::SmallButton("0.6") ) { depthSigma = 0.6f; parametersChanged = true; }

    ImGui::Separator();

    ImGui::Text("Iterations (Number of filter passes)");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "More iterations = smoother but slower");
    if ( ImGui::SliderInt("##Iterations", &iterations, 1, 8) ) {
        SetDenoiserIterations(iterations);
    }

    ImGui::Separator();

    ImGui::Text("Presets:");
    if ( ImGui::Button("Conservative (Sharp)") ) {
        colorSigma = 0.25f;
        normalSigma = 12.0f;
        depthSigma = 0.15f;
        iterations = 4;
        parametersChanged = true;
        SetDenoiserIterations(iterations);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Balanced (Default)") ) {
        colorSigma = 0.15f;
        normalSigma = 16.0f;
        depthSigma = 0.1f;
        iterations = 5;
        parametersChanged = true;
        SetDenoiserIterations(iterations);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Aggressive (Smooth)") ) {
        colorSigma = 0.7f;
        normalSigma = 4.0f;
        depthSigma = 0.5f;
        iterations = 6;
        parametersChanged = true;
        SetDenoiserIterations(iterations);
    }

    if ( parametersChanged ) {
        SetDenoiserParameters(colorSigma, normalSigma, depthSigma);
    }

    ImGui::Separator();

    ImGui::Text("Current Values:");
    ImGui::Text("  Color: %.3f, Normal: %.1f, Depth: %.3f", colorSigma, normalSigma, depthSigma);
    ImGui::Text("  Iterations: %d, Enabled: %s", iterations, denoiserEnabled ? "Yes" : "No");

    ImGui::End();
    */
    ImGui::Begin("Debug Buffer");

    UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_GPU_DESCRIPTOR_HANDLE baseGpuHandle = m_debugSrvHeapStart_GPU;
    ImVec2 imageSize(m_width / 3.0f, m_height / 3.0f);

    ImGui::Text("--- Temporal Accumulation ---");

    CD3DX12_GPU_DESCRIPTOR_HANDLE renderTargetHandle = baseGpuHandle;
    ImGui::Text("Final Output (u0) - Temporal Accumulated");
    ImGui::Image((ImTextureID)renderTargetHandle.ptr, imageSize);

    ImGui::Separator();

    CD3DX12_GPU_DESCRIPTOR_HANDLE accumulationHandle = baseGpuHandle;
    accumulationHandle.Offset(1, descriptorSize);
    ImGui::Text("Accumulation Buffer (u1)");
    ImGui::Image((ImTextureID)accumulationHandle.ptr, imageSize);

    ImGui::Separator();

    CD3DX12_GPU_DESCRIPTOR_HANDLE prevFrameHandle = baseGpuHandle;
    prevFrameHandle.Offset(2, descriptorSize);
    ImGui::Text("Prev Frame Data (u2)");
    ImGui::Image((ImTextureID)prevFrameHandle.ptr, imageSize);

    ImGui::Separator();

    ImGui::Text("--- G-Buffers ---");

    CD3DX12_GPU_DESCRIPTOR_HANDLE albedoHandle = baseGpuHandle;
    albedoHandle.Offset(3, descriptorSize);
    ImGui::Text("Albedo (u3)");
    ImGui::Image((ImTextureID)albedoHandle.ptr, imageSize);

    ImGui::Separator();

    CD3DX12_GPU_DESCRIPTOR_HANDLE normalHandle = baseGpuHandle;
    normalHandle.Offset(4, descriptorSize);
    ImGui::Text("Normal (u4)");
    ImGui::Image((ImTextureID)normalHandle.ptr, imageSize);

    ImGui::Separator();

    CD3DX12_GPU_DESCRIPTOR_HANDLE depthHandle = baseGpuHandle;
    depthHandle.Offset(5, descriptorSize);
    ImGui::Text("Depth (u5)");
    ImGui::Image((ImTextureID)depthHandle.ptr, imageSize);

    ImGui::Separator();

    ImGui::Text("--- Denoiser ---");

    CD3DX12_GPU_DESCRIPTOR_HANDLE denoisedHandle = baseGpuHandle;
    denoisedHandle.Offset(6, descriptorSize);
    if ( m_denoiserEnabled ) {
        ImGui::Text("Denoised Output (u6)");
        ImGui::Image((ImTextureID)denoisedHandle.ptr, imageSize);
    } else {
        ImGui::Text("Denoiser Disabled");
    }

    ImGui::End();

    std::vector<CD3DX12_RESOURCE_BARRIER> restoreBarriers;

    restoreBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    restoreBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_accumulationBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    restoreBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_prevFrameDataBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    restoreBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_albedoBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    restoreBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_normalBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    restoreBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_depthBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    if ( m_denoiserEnabled ) {
        restoreBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_denoisedOutput.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    }

    if ( !restoreBarriers.empty() ) {
        m_commandList->ResourceBarrier(static_cast<UINT>( restoreBarriers.size() ), restoreBarriers.data());
    }
}

void DXRRenderer::InitializeDXR(ID3D12Device* device) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));

    if ( FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED ) {
        ThrowIfFailed(hr);
    }
}

void DXRRenderer::CreateRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE descriptorRanges[2];

    descriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 9, 0);

    descriptorRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 9, 1);

    CD3DX12_ROOT_PARAMETER rootParameters[5];
    rootParameters[0].InitAsDescriptorTable(1, &descriptorRanges[0]);
    rootParameters[1].InitAsShaderResourceView(0);
    rootParameters[2].InitAsConstantBufferView(0);
    rootParameters[3].InitAsDescriptorTable(1, &descriptorRanges[1]);
    rootParameters[4].InitAsConstantBufferView(1);

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

    char debugMsg[256];
    sprintf_s(debugMsg, "Global root signature created with temporal accumulation + denoiser support (5 parameters, 7 UAVs)\n");
    OutputDebugStringA(debugMsg);
}

void DXRRenderer::CreateRaytracingPipelineStateObject() {
    auto rayGenShader = LoadOrCompileShader(L"Src/Shader/EntryPoint/RayGen.hlsl", L"RayGen");
    auto missShader = LoadOrCompileShader(L"Src/Shader/EntryPoint/Miss.hlsl", L"Miss");
    auto lambertianHitShader = LoadOrCompileShader(L"Src/Shader/EntryPoint/ClosestHit_Lambertian.hlsl", L"ClosestHit_Lambertian");
    auto metalHitShader = LoadOrCompileShader(L"Src/Shader/EntryPoint/ClosestHit_Metal.hlsl", L"ClosestHit_Metal");
    auto dielectricHitShader = LoadOrCompileShader(L"Src/Shader/EntryPoint/ClosestHit_Dielectric.hlsl", L"ClosestHit_Dielectric");
    auto lightHitShader = LoadOrCompileShader(L"Src/Shader/EntryPoint/ClosestHit_DiffuseLight.hlsl", L"ClosestHit_DiffuseLight");

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

    D3D12_EXPORT_DESC exportDescs[6];
    D3D12_DXIL_LIBRARY_DESC dxilLibDescs[6];
    D3D12_HIT_GROUP_DESC hitGroupDescs[4];
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfigDesc = {};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderConfigAssociationDesc = {};

    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.reserve(16);

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


    shaderConfigDesc.MaxPayloadSizeInBytes = 72;
    shaderConfigDesc.MaxAttributeSizeInBytes = sizeof(float) * 2;

    D3D12_STATE_SUBOBJECT shaderConfigSubobject = {};
    shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigSubobject.pDesc = &shaderConfigDesc;
    subobjects.push_back(shaderConfigSubobject);

    shaderConfigAssociationDesc.pSubobjectToAssociate = &subobjects.back();
    shaderConfigAssociationDesc.NumExports = _countof(exportNames);
    shaderConfigAssociationDesc.pExports = exportNames;

    D3D12_STATE_SUBOBJECT shaderConfigAssociationSubobject = {};
    shaderConfigAssociationSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    shaderConfigAssociationSubobject.pDesc = &shaderConfigAssociationDesc;
    subobjects.push_back(shaderConfigAssociationSubobject);

    D3D12_STATE_SUBOBJECT globalRootSigSubobject = {};
    globalRootSigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    globalRootSigSubobject.pDesc = m_globalRootSignature.GetAddressOf();
    subobjects.push_back(globalRootSigSubobject);

    static D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = { 8 };

    D3D12_STATE_SUBOBJECT pipelineConfigSubobject = {};
    pipelineConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigSubobject.pDesc = &pipelineConfig;
    subobjects.push_back(pipelineConfigSubobject);

    D3D12_STATE_OBJECT_DESC raytracingPipelineDesc = {};
    raytracingPipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    raytracingPipelineDesc.NumSubobjects = static_cast<UINT>( subobjects.size() );
    raytracingPipelineDesc.pSubobjects = subobjects.data();

    HRESULT hr = m_device->CreateStateObject(&raytracingPipelineDesc, IID_PPV_ARGS(&m_rtStateObject));
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create raytracing pipeline state object");
    }

    char debugMsg[256];
    sprintf_s(debugMsg, "Raytracing pipeline created with payload size: %d bytes\n",
        shaderConfigDesc.MaxPayloadSizeInBytes);
    OutputDebugStringA(debugMsg);
}

ComPtr<IDxcBlob> DXRRenderer::LoadCSO(const std::wstring& filename) {
    std::ifstream file(filename, std::ios::binary);
    if ( !file.is_open() ) {
        std::string errorMsg = "Failed to open shader file: ";
        std::string filenameStr(filename.begin(), filename.end());
        throw std::runtime_error(errorMsg + filenameStr);
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>( data.data() ), fileSize);
    file.close();

    if ( !m_dxcUtils ) {
        HRESULT hr_init = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_dxcUtils));
        if ( FAILED(hr_init) ) {
            throw std::runtime_error("Failed to create DxcUtils instance in LoadCSO");
        }
    }

    ComPtr<IDxcBlobEncoding> blobEncoding;
    HRESULT hr = m_dxcUtils->CreateBlob(
        data.data(),
        static_cast<UINT32>( data.size() ),
        DXC_CP_UTF8,
        blobEncoding.GetAddressOf()
    );

    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to create blob from shader data using IDxcUtils::CreateBlob");
    }

    ComPtr<IDxcBlob> resultBlob;
    hr = blobEncoding.As(&resultBlob);
    if ( FAILED(hr) ) {
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

    XMVECTOR eyePos = XMLoadFloat3(&cameraData.position);
    XMVECTOR targetPos = XMLoadFloat3(&cameraData.target);
    XMVECTOR upVec = XMLoadFloat3(&cameraData.up);
    XMMATRIX viewMatrix = XMMatrixInverse(nullptr, XMMatrixLookAtLH(eyePos, targetPos, upVec));
    XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(cameraData.fov, cameraData.aspect, 0.1f, 1000.0f);

    XMVECTOR rightVec = XMVector3Normalize(XMVectorSet(
        XMVectorGetX(viewMatrix.r[0]),
        XMVectorGetY(viewMatrix.r[0]),
        XMVectorGetZ(viewMatrix.r[0]),
        0.0f
    ));

    XMVECTOR upVector = XMVector3Normalize(XMVectorSet(
        XMVectorGetX(viewMatrix.r[1]),
        XMVectorGetY(viewMatrix.r[1]),
        XMVectorGetZ(viewMatrix.r[1]),
        0.0f
    ));

    XMVECTOR forwardVec = XMVector3Normalize(XMVectorSet(
        XMVectorGetX(viewMatrix.r[2]),
        XMVectorGetY(viewMatrix.r[2]),
        XMVectorGetZ(viewMatrix.r[2]),
        0.0f
    ));

    float tanHalfFov = tanf(cameraData.fov * 0.5f);
    float aspectRatio = cameraData.aspect;

    float distToSphere1 = sqrtf(powf(190.0f - cameraData.position.x, 2) +
        powf(90.0f - cameraData.position.y, 2) +
        powf(190.0f - cameraData.position.z, 2));

    SceneConstantBuffer sceneConstants;
    sceneConstants.projectionMatrix = projMatrix;
    sceneConstants.viewMatrix = viewMatrix;

    XMStoreFloat3(&sceneConstants.cameraRight, rightVec);
    XMStoreFloat3(&sceneConstants.cameraUp, upVector);
    XMStoreFloat3(&sceneConstants.cameraForward, forwardVec);
    sceneConstants.tanHalfFov = tanHalfFov;
    sceneConstants.aspectRatio = aspectRatio;

    static uint32_t frameCounter = 0;
    sceneConstants.frameCount = static_cast<float>( frameCounter++ );
    
    static XMFLOAT3 prevCameraPos = { 0.0f, 0.0f, 0.0f };
    static XMFLOAT4X4 prevViewMatrix = {};
    static bool firstFrame = true;
    
    bool cameraMoved = false;
    if (!firstFrame) {
        float positionDelta = sqrtf(
            powf(cameraData.position.x - prevCameraPos.x, 2) +
            powf(cameraData.position.y - prevCameraPos.y, 2) +
            powf(cameraData.position.z - prevCameraPos.z, 2)
        );
        
        XMFLOAT4X4 currentViewMatrix;
        XMStoreFloat4x4(&currentViewMatrix, viewMatrix);
        
        float matrixDelta = 0.0f;
        for (int i = 0; i < 16; i++) {
            float diff = reinterpret_cast<float*>(&currentViewMatrix)[i] - reinterpret_cast<float*>(&prevViewMatrix)[i];
            matrixDelta += diff * diff;
        }
        matrixDelta = sqrtf(matrixDelta);
        
        const float POSITION_THRESHOLD = 0.01f;
        const float MATRIX_THRESHOLD = 0.001f;
        
        cameraMoved = (positionDelta > POSITION_THRESHOLD) || (matrixDelta > MATRIX_THRESHOLD);
    }
    
    prevCameraPos = cameraData.position;
    XMStoreFloat4x4(&prevViewMatrix, viewMatrix);
    firstFrame = false;
    
    sceneConstants.numLights = m_numLights;
    sceneConstants.cameraMovedFlag = cameraMoved ? 1u : 0u;

    void* mappedData;
    m_sceneConstantBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, &sceneConstants, sizeof(SceneConstantBuffer));
    m_sceneConstantBuffer->Unmap(0, nullptr);

    char debugMsg[512];
    sprintf_s(debugMsg, "Camera update: FOV=%.2f, Aspect=%.2f, TanHalfFOV=%.4f\n",
        cameraData.fov, aspectRatio, tanHalfFov);
    OutputDebugStringA(debugMsg);
}


void DXRRenderer::CreateAccelerationStructures() {
    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );

    if ( !dxrScene ) {
        OutputDebugStringA("ERROR: No DXRScene found!\n");
        return;
    }

    TLASData tlasData = dxrScene->GetTLASData();

    char debugMsg[512];
    sprintf_s(debugMsg, "=== Acceleration Structure Detailed Debug ===\n");
    OutputDebugStringA(debugMsg);
    sprintf_s(debugMsg, "Total BLAS count: %zu\n", tlasData.blasDataList.size());
    OutputDebugStringA(debugMsg);

    if ( tlasData.blasDataList.empty() ) {
        OutputDebugStringA("ERROR: No BLAS data found! No objects to render.\n");
        return;
    }

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

    std::vector<CD3DX12_RESOURCE_BARRIER> blasBarriers;
    for ( auto& blas : m_bottomLevelAS ) {
        blasBarriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(blas.Get()));
    }
    if ( !blasBarriers.empty() ) {
        m_commandList->ResourceBarrier(static_cast<UINT>( blasBarriers.size() ), blasBarriers.data());
    }

    OutputDebugStringA("Creating TLAS with embedded materials...\n");
    CreateTLAS(tlasData);


    Singleton<Renderer>::getInstance().ExecuteCommandListAndWait();
    OutputDebugStringA("Acceleration structures created successfully\n");
}

void DXRRenderer::CreateBLAS(BLASData& blasData, ComPtr<ID3D12Resource>& blasBuffer) {
    HRESULT hr;

    char debugMsg[512];
    sprintf_s(debugMsg, "=== Creating BLAS (FIXED) ===\n");
    OutputDebugStringA(debugMsg);

    if ( blasData.vertices.empty() || blasData.indices.empty() ) {
        OutputDebugStringA("ERROR: Empty vertices or indices in BLAS!\n");
        return;
    }

    sprintf_s(debugMsg, "Input data: %zu vertices, %zu indices\n",
        blasData.vertices.size(), blasData.indices.size());
    OutputDebugStringA(debugMsg);

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

    void* mappedVertexData;
    hr = blasData.vertexBuffer->Map(0, nullptr, &mappedVertexData);
    if ( FAILED(hr) ) {
        OutputDebugStringA("ERROR: Failed to map vertex buffer\n");
        throw std::runtime_error("Failed to map vertex buffer");
    }

    memcpy(mappedVertexData, blasData.vertices.data(), vertexBufferSize);
    blasData.vertexBuffer->Unmap(0, nullptr);

    OutputDebugStringA("Vertex buffer uploaded successfully\n");

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

    void* mappedIndexData;
    hr = blasData.indexBuffer->Map(0, nullptr, &mappedIndexData);
    if ( FAILED(hr) ) {
        OutputDebugStringA("ERROR: Failed to map index buffer\n");
        throw std::runtime_error("Failed to map index buffer");
    }

    memcpy(mappedIndexData, blasData.indices.data(), indexBufferSize);
    blasData.indexBuffer->Unmap(0, nullptr);

    OutputDebugStringA("Index buffer uploaded successfully\n");

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    geometryDesc.Triangles.VertexBuffer.StartAddress = blasData.vertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(DXRVertex);
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Triangles.VertexCount = static_cast<UINT>( blasData.vertices.size() );

    geometryDesc.Triangles.IndexBuffer = blasData.indexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geometryDesc.Triangles.IndexCount = static_cast<UINT>( blasData.indices.size() );

    sprintf_s(debugMsg, "Geometry setup: VertexCount=%u, IndexCount=%u\n",
        geometryDesc.Triangles.VertexCount, geometryDesc.Triangles.IndexCount);
    sprintf_s(debugMsg, "Expected triangles: %u\n", geometryDesc.Triangles.IndexCount / 3);
    OutputDebugStringA(debugMsg);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
    blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    blasInputs.NumDescs = 1;
    blasInputs.pGeometryDescs = &geometryDesc;

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

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

    for ( size_t i = 0; i < tlasData.blasDataList.size(); ++i ) {
        const auto& blasData = tlasData.blasDataList[i];

        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};

        XMMATRIX transform = blasData.transform;

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

        XMFLOAT3X4 transform3x4;
        XMStoreFloat3x4(&transform3x4, transform);

        for ( int row = 0; row < 3; ++row ) {
            for ( int col = 0; col < 4; ++col ) {
                instanceDesc.Transform[row][col] = transform3x4.m[row][col];
            }
        }

        instanceDesc.InstanceID = static_cast<UINT>( i );
        instanceDesc.InstanceMask = 0xFF;

        auto& gameManager = Singleton<GameManager>::getInstance();
        DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );
        const auto& materialData = dxrScene->GetUniqueMaterials();

        instanceDesc.InstanceContributionToHitGroupIndex = static_cast<UINT>( materialData[blasData.materialID].materialType );
        instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc.AccelerationStructure = m_bottomLevelAS[i]->GetGPUVirtualAddress();

        instanceDescs.push_back(instanceDesc);
    }

    CreateMaterialBuffer(tlasData);
    
    CreateLightBuffer(tlasData);

    CreateVertexIndexBuffers(tlasData);

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


void DXRRenderer::CreateVertexIndexBuffers(const TLASData& tlasData) {
    char debugMsg[256];
    sprintf_s(debugMsg, "=== Creating FIXED Unified Global Buffers (SPHERE-SAFE) ===\n");
    OutputDebugStringA(debugMsg);

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

        vertexOffsets.push_back(currentVertexOffset);
        indexOffsets.push_back(currentIndexOffset);

        sprintf_s(debugMsg, "Instance[%zu]: %zu vertices, %zu indices\n",
            i, blasData.vertices.size(), blasData.indices.size());
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "  -> Will be placed at vertexOffset=%u, indexOffset=%u\n",
            currentVertexOffset, currentIndexOffset);
        OutputDebugStringA(debugMsg);

        allVertices.insert(allVertices.end(), blasData.vertices.begin(), blasData.vertices.end());

        allIndices.insert(allIndices.end(), blasData.indices.begin(), blasData.indices.end());

        sprintf_s(debugMsg, "  Original indices (first 6): ");
        OutputDebugStringA(debugMsg);
        for ( size_t j = 0; j < min(6, blasData.indices.size()); ++j ) {
            sprintf_s(debugMsg, "%u ", blasData.indices[j]);
            OutputDebugStringA(debugMsg);
        }
        sprintf_s(debugMsg, "\n");
        OutputDebugStringA(debugMsg);

        currentVertexOffset += static_cast<uint32_t>( blasData.vertices.size() );
        currentIndexOffset += static_cast<uint32_t>( blasData.indices.size() );
    }

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

    CreateInstanceOffsetBuffer(tlasData, vertexOffsets, indexOffsets);

    sprintf_s(debugMsg, "=== Unified Buffer Validation (SPHERE-SAFE) ===\n");
    OutputDebugStringA(debugMsg);

    for ( size_t i = 0; i < min(3, tlasData.blasDataList.size()); ++i ) {
        uint32_t vOffset = vertexOffsets[i];
        uint32_t iOffset = indexOffsets[i];

        sprintf_s(debugMsg, "Instance[%zu]: vOffset=%u, iOffset=%u\n", i, vOffset, iOffset);
        OutputDebugStringA(debugMsg);

        if ( iOffset + 2 < allIndices.size() ) {
            uint32_t i0 = allIndices[iOffset + 0] + vOffset;
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

void DXRRenderer::CreateInstanceOffsetBuffer(const TLASData& tlasData,
    const std::vector<uint32_t>& vertexOffsets,
    const std::vector<uint32_t>& indexOffsets) {
    struct InstanceOffsetData {
        uint32_t vertexOffset;
        uint32_t indexOffset;
        uint32_t materialID;
        uint32_t padding;
    };

    std::vector<InstanceOffsetData> offsetArray;
    offsetArray.reserve(tlasData.blasDataList.size());

    for ( size_t i = 0; i < tlasData.blasDataList.size(); ++i ) {
        InstanceOffsetData data = {};
        data.vertexOffset = vertexOffsets[i];
        data.indexOffset = indexOffsets[i];
        data.materialID = tlasData.blasDataList[i].materialID;
        offsetArray.push_back(data);
    }

    char debugMsg[256];
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

void DXRRenderer::CreateDebugBufferViews() {
    UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_debugSrvHeapStart_CPU;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    srvDesc.Format = m_raytracingOutput->GetDesc().Format;
    m_device->CreateShaderResourceView(m_raytracingOutput.Get(), &srvDesc, cpuHandle);
    cpuHandle.Offset(1, descriptorSize);

    srvDesc.Format = m_accumulationBuffer->GetDesc().Format;
    m_device->CreateShaderResourceView(m_accumulationBuffer.Get(), &srvDesc, cpuHandle);
    cpuHandle.Offset(1, descriptorSize);

    srvDesc.Format = m_prevFrameDataBuffer->GetDesc().Format;
    m_device->CreateShaderResourceView(m_prevFrameDataBuffer.Get(), &srvDesc, cpuHandle);
    cpuHandle.Offset(1, descriptorSize);

    srvDesc.Format = m_albedoBuffer->GetDesc().Format;
    m_device->CreateShaderResourceView(m_albedoBuffer.Get(), &srvDesc, cpuHandle);
    cpuHandle.Offset(1, descriptorSize);

    srvDesc.Format = m_normalBuffer->GetDesc().Format;
    m_device->CreateShaderResourceView(m_normalBuffer.Get(), &srvDesc, cpuHandle);
    cpuHandle.Offset(1, descriptorSize);

    srvDesc.Format = m_depthBuffer->GetDesc().Format;
    m_device->CreateShaderResourceView(m_depthBuffer.Get(), &srvDesc, cpuHandle);
    cpuHandle.Offset(1, descriptorSize);

    srvDesc.Format = m_denoisedOutput->GetDesc().Format;
    m_device->CreateShaderResourceView(m_denoisedOutput.Get(), &srvDesc, cpuHandle);
}

void DXRRenderer::CreateDenoiserResources() {
    char debugMsg[256];
    sprintf_s(debugMsg, "Creating denoiser resources...\n");
    OutputDebugStringA(debugMsg);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(DenoiserConstants));
    HRESULT hr = m_device->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_denoiserConstants));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create denoiser constants");

    OutputDebugStringA("Denoiser resources created successfully\n");
}

void DXRRenderer::CreateDenoiserPipeline() {
    char debugMsg[256];
    sprintf_s(debugMsg, "Creating denoiser pipeline...\n");
    OutputDebugStringA(debugMsg);

    auto denoiserShader = LoadOrCompileShader(L"Src/Shader/PostProcess/ATrousDenoiser.hlsl", L"CSMain", L"cs_6_5");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_globalRootSignature.Get();
    psoDesc.CS.pShaderBytecode = denoiserShader->GetBufferPointer();
    psoDesc.CS.BytecodeLength = denoiserShader->GetBufferSize();

    HRESULT hr = m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_denoiserPSO));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create denoiser pipeline state");

    OutputDebugStringA("Denoiser pipeline created successfully using global root signature\n");
}

ComPtr<ID3D12Resource> DXRRenderer::RunDenoiser() {
    if ( !m_denoiserEnabled || m_denoiserIterations <= 0 ) {
        return m_raytracingOutput.Get();
    }

    m_commandList->SetPipelineState(m_denoiserPSO.Get());
    m_commandList->SetComputeRootSignature(m_globalRootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE tableBaseHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    m_commandList->SetComputeRootDescriptorTable(0, tableBaseHandle);
    m_commandList->SetComputeRootShaderResourceView(1, m_topLevelAS->GetGPUVirtualAddress());
    m_commandList->SetComputeRootConstantBufferView(2, m_sceneConstantBuffer->GetGPUVirtualAddress());
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvTableHandle = tableBaseHandle;
    srvTableHandle.Offset(9, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_commandList->SetComputeRootDescriptorTable(3, srvTableHandle);


    std::vector<CD3DX12_RESOURCE_BARRIER> gbufferToSrvBarriers = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_albedoBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_normalBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_depthBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
    };
    m_commandList->ResourceBarrier(static_cast<UINT>( gbufferToSrvBarriers.size() ), gbufferToSrvBarriers.data());

    auto inputToSrvBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &inputToSrvBarrier);

    for ( int iteration = 0; iteration < m_denoiserIterations; ++iteration ) {
        auto uavBarrierForOutput = CD3DX12_RESOURCE_BARRIER::UAV(m_denoisedOutput.Get());
        m_commandList->ResourceBarrier(1, &uavBarrierForOutput);

        UpdateDenoiserConstants(1 << iteration);
        m_commandList->SetComputeRootConstantBufferView(4, m_denoiserConstants->GetGPUVirtualAddress());
        UINT groupsX = ( m_width + 7 ) / 8;
        UINT groupsY = ( m_height + 7 ) / 8;
        m_commandList->Dispatch(groupsX, groupsY, 1);

        auto uavBarrierAfterWrite = CD3DX12_RESOURCE_BARRIER::UAV(m_denoisedOutput.Get());
        m_commandList->ResourceBarrier(1, &uavBarrierAfterWrite);

        if ( iteration < m_denoiserIterations - 1 ) {
            std::vector<CD3DX12_RESOURCE_BARRIER> copyBarriers = {
                CD3DX12_RESOURCE_BARRIER::Transition(m_denoisedOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
            };
            m_commandList->ResourceBarrier(static_cast<UINT>( copyBarriers.size() ), copyBarriers.data());

            m_commandList->CopyResource(m_raytracingOutput.Get(), m_denoisedOutput.Get());

            std::vector<CD3DX12_RESOURCE_BARRIER> restoreBarriers = {
                CD3DX12_RESOURCE_BARRIER::Transition(m_denoisedOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
            };
            m_commandList->ResourceBarrier(static_cast<UINT>( restoreBarriers.size() ), restoreBarriers.data());
        }
    }

    std::vector<CD3DX12_RESOURCE_BARRIER> postLoopBarriers = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(m_albedoBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(m_normalBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(m_depthBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    };
    m_commandList->ResourceBarrier(static_cast<UINT>( postLoopBarriers.size() ), postLoopBarriers.data());

    return m_denoisedOutput;
}

void DXRRenderer::UpdateDenoiserConstants(int stepSize) {
    DenoiserConstants constants = {};
    constants.stepSize = stepSize;
    constants.colorSigma = m_colorSigma;
    constants.normalSigma = m_normalSigma;
    constants.depthSigma = m_depthSigma;
    constants.texelSize = DirectX::XMFLOAT2(1.0f / m_width, 1.0f / m_height);
    constants.padding = DirectX::XMFLOAT2(0.0f, 0.0f);

    void* mappedData;
    HRESULT hr = m_denoiserConstants->Map(0, nullptr, &mappedData);
    if ( SUCCEEDED(hr) ) {
        memcpy(mappedData, &constants, sizeof(DenoiserConstants));
        m_denoiserConstants->Unmap(0, nullptr);
    }
}

ComPtr<IDxcBlob> DXRRenderer::CompileShaderFromFile(const std::wstring& hlslPath, const std::wstring& entryPoint, const std::wstring& target) {
    ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = m_dxcUtils->LoadFile(hlslPath.c_str(), nullptr, &sourceBlob);
    if ( FAILED(hr) ) {
        std::string errorMsg = "Failed to load HLSL file: ";
        std::string filenameStr(hlslPath.begin(), hlslPath.end());
        throw std::runtime_error(errorMsg + filenameStr);
    }

    std::vector<LPCWSTR> arguments = {
        hlslPath.c_str(),
        L"-E", entryPoint.c_str(),
        L"-T", target.c_str(),
        L"-O3",
        L"-Qstrip_debug",
        L"-Qstrip_reflect",
    };

    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = CP_UTF8;

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

    HRESULT compileHR;
    compileResult->GetStatus(&compileHR);

    if ( FAILED(compileHR) ) {
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
    if ( lastSlash != std::wstring::npos ) {
        csoPath = csoPath.substr(lastSlash + 1);
    }
    size_t lastDot = csoPath.find_last_of(L'.');
    if ( lastDot != std::wstring::npos ) {
        csoPath = csoPath.substr(0, lastDot) + L".cso";
    }
    else {
        csoPath += L".cso";
    }
    csoPath = L"Shader/" + csoPath;

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

    WIN32_FILE_ATTRIBUTE_DATA hlslAttr, csoAttr;
    bool hlslExists = GetFileAttributesExW(hlslPath.c_str(), GetFileExInfoStandard, &hlslAttr);
    bool csoExists = GetFileAttributesExW(csoPath.c_str(), GetFileExInfoStandard, &csoAttr);

    bool needsRecompile = !csoExists || !hlslExists;
    if ( hlslExists && csoExists ) {
        FILETIME hlslTime = hlslAttr.ftLastWriteTime;
        FILETIME csoTime = csoAttr.ftLastWriteTime;
        needsRecompile = CompareFileTime(&hlslTime, &csoTime) > 0;
    }

    if ( needsRecompile && hlslExists ) {
        std::wcout << L"Compiling shader: " << hlslPath << L" -> " << csoPath << std::endl;

        ComPtr<IDxcBlob> compiledBlob = CompileShaderFromFile(hlslPath, entryPoint, target);

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
        std::wcout << L"Loading cached shader: " << csoPath << std::endl;
        return LoadCSO(csoPath);
    }
    else {
        throw std::runtime_error("Neither HLSL source nor CSO file found");
    }
}

void DXRRenderer::CreateOutputResource() {
    char debugMsg[256];
    sprintf_s(debugMsg, "Creating output resources and ALL descriptors...\n");
    OutputDebugStringA(debugMsg);

    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);

    CD3DX12_RESOURCE_DESC outputResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM, m_width, m_height, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    HRESULT hr = m_device->CreateCommittedResource(
        &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &outputResourceDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
        IID_PPV_ARGS(&m_raytracingOutput));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create raytracing output resource");

    CD3DX12_RESOURCE_DESC accumulationDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R32G32B32A32_FLOAT, m_width, m_height, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    hr = m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &accumulationDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_accumulationBuffer));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create accumulation buffer");
    
    hr = m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &accumulationDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_prevFrameDataBuffer));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create prev frame data buffer");

    CD3DX12_RESOURCE_DESC gbufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R32G32B32A32_FLOAT, m_width, m_height, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    hr = m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &gbufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_albedoBuffer));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create albedo buffer");
    hr = m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &gbufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_normalBuffer));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create normal buffer");
    hr = m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &gbufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_depthBuffer));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create depth buffer");

    hr = m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &outputResourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_denoisedOutput));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create denoised output");

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.NumDescriptors = 18;
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create descriptor heap");

    UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &uavDesc, handle);
    handle.Offset(1, descriptorSize);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    m_device->CreateUnorderedAccessView(m_accumulationBuffer.Get(), nullptr, &uavDesc, handle);
    handle.Offset(1, descriptorSize);

    m_device->CreateUnorderedAccessView(m_prevFrameDataBuffer.Get(), nullptr, &uavDesc, handle);
    handle.Offset(1, descriptorSize);

    m_device->CreateUnorderedAccessView(m_albedoBuffer.Get(), nullptr, &uavDesc, handle);
    handle.Offset(1, descriptorSize);

    m_device->CreateUnorderedAccessView(m_normalBuffer.Get(), nullptr, &uavDesc, handle);
    handle.Offset(1, descriptorSize);

    m_device->CreateUnorderedAccessView(m_depthBuffer.Get(), nullptr, &uavDesc, handle);
    handle.Offset(1, descriptorSize);

    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &uavDesc, handle);
    handle.Offset(1, descriptorSize);

    handle.Offset(2, descriptorSize);


    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );
    if ( !dxrScene ) throw std::runtime_error("DXRScene not found during descriptor creation.");
    const auto& materials = dxrScene->GetUniqueMaterials();
    TLASData tlasData = dxrScene->GetTLASData();

    if ( m_materialBuffer ) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>( materials.size() );
        srvDesc.Buffer.StructureByteStride = sizeof(DXRMaterialData);
        m_device->CreateShaderResourceView(m_materialBuffer.Get(), &srvDesc, handle);
    }
    handle.Offset(1, descriptorSize);

    if ( m_globalVertexBuffer ) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = m_totalVertexCount;
        srvDesc.Buffer.StructureByteStride = sizeof(DXRVertex);
        m_device->CreateShaderResourceView(m_globalVertexBuffer.Get(), &srvDesc, handle);
    }
    handle.Offset(1, descriptorSize);

    if ( m_globalIndexBuffer ) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_UINT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = m_totalIndexCount;
        m_device->CreateShaderResourceView(m_globalIndexBuffer.Get(), &srvDesc, handle);
    }
    handle.Offset(1, descriptorSize);

    if ( m_instanceOffsetBuffer ) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>( tlasData.blasDataList.size() );
        srvDesc.Buffer.StructureByteStride = 16;
        m_device->CreateShaderResourceView(m_instanceOffsetBuffer.Get(), &srvDesc, handle);
    }
    handle.Offset(1, descriptorSize);

    if (m_lightBuffer && m_numLights > 0) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = m_numLights;
        srvDesc.Buffer.StructureByteStride = sizeof(DXRLightData);
        m_device->CreateShaderResourceView(m_lightBuffer.Get(), &srvDesc, handle);
    }
    handle.Offset(1, descriptorSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC gbufferSrvDesc = {};
    gbufferSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    gbufferSrvDesc.Texture2D.MipLevels = 1;
    gbufferSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    gbufferSrvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    m_device->CreateShaderResourceView(m_albedoBuffer.Get(), &gbufferSrvDesc, handle);
    handle.Offset(1, descriptorSize);

    m_device->CreateShaderResourceView(m_normalBuffer.Get(), &gbufferSrvDesc, handle);
    handle.Offset(1, descriptorSize);

    m_device->CreateShaderResourceView(m_depthBuffer.Get(), &gbufferSrvDesc, handle);
    handle.Offset(1, descriptorSize);


    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneConstantBuffer));
    hr = m_device->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_sceneConstantBuffer));
    if ( FAILED(hr) ) throw std::runtime_error("Failed to create scene constant buffer");

    OutputDebugStringA("Output resources and ALL descriptors created CORRECTLY\n");
}

void DXRRenderer::CreateMaterialBuffer(const TLASData& tlasData) {
    if ( tlasData.blasDataList.empty() ) return;

    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );
    if ( !dxrScene ) return;

    const auto& materials = dxrScene->GetUniqueMaterials();
    if ( materials.empty() ) return;

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

void DXRRenderer::CollectLightsFromScene(const TLASData& tlasData) {
    if (tlasData.blasDataList.empty()) return;

    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>(gameManager.GetScene().get());
    if (!dxrScene) return;

    const auto& materials = dxrScene->GetUniqueMaterials();
    if (materials.empty()) return;

    m_lightData.clear();
    
    for (size_t instanceIdx = 0; instanceIdx < tlasData.blasDataList.size(); ++instanceIdx) {
        const auto& blasData = tlasData.blasDataList[instanceIdx];
        
        if (blasData.materialID < materials.size()) {
            const auto& material = materials[blasData.materialID];
            
            if (material.materialType == 3) {
                DXRLightData lightData = {};
                
                XMFLOAT3 position;
                XMStoreFloat3(&position, blasData.transform.r[3]);
                lightData.position = position;
                
                lightData.emission = material.emission;
                
                if (!blasData.vertices.empty()) {
                    XMFLOAT3 minPos = blasData.vertices[0].position;
                    XMFLOAT3 maxPos = blasData.vertices[0].position;
                    
                    for (const auto& vertex : blasData.vertices) {
                        minPos.x = min(minPos.x, vertex.position.x);
                        minPos.y = min(minPos.y, vertex.position.y);
                        minPos.z = min(minPos.z, vertex.position.z);
                        maxPos.x = max(maxPos.x, vertex.position.x);
                        maxPos.y = max(maxPos.y, vertex.position.y);
                        maxPos.z = max(maxPos.z, vertex.position.z);
                    }
                    
                    lightData.size = XMFLOAT3(
                        maxPos.x - minPos.x,
                        maxPos.y - minPos.y,
                        maxPos.z - minPos.z
                    );
                    
                    lightData.area = lightData.size.x * lightData.size.z;
                    
                    lightData.normal = XMFLOAT3(0.0f, -1.0f, 0.0f);
                } else {
                    lightData.size = XMFLOAT3(1.0f, 1.0f, 1.0f);
                    lightData.area = 1.0f;
                    lightData.normal = XMFLOAT3(0.0f, -1.0f, 0.0f);
                }
                
                lightData.lightType = 0;
                lightData.instanceID = static_cast<uint32_t>(instanceIdx);
                
                m_lightData.push_back(lightData);
                
                char debugMsg[256];
                sprintf_s(debugMsg, "Light found: Instance %zu, Position(%.2f, %.2f, %.2f), Emission(%.2f, %.2f, %.2f)\n",
                    instanceIdx, position.x, position.y, position.z,
                    material.emission.x, material.emission.y, material.emission.z);
                OutputDebugStringA(debugMsg);
            }
        }
    }
    
    m_numLights = static_cast<uint32_t>(m_lightData.size());
    
    char debugMsg[256];
    sprintf_s(debugMsg, "Total lights collected: %u\n", m_numLights);
    OutputDebugStringA(debugMsg);
}

void DXRRenderer::CreateLightBuffer(const TLASData& tlasData) {
    CollectLightsFromScene(tlasData);
    
    if (m_lightData.empty()) {
        OutputDebugStringA("No lights found in scene\n");
        return;
    }

    UINT lightBufferSize = static_cast<UINT>(m_lightData.size() * sizeof(DXRLightData));

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC lightBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(lightBufferSize);

    HRESULT hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &lightBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_lightBuffer)
    );

    if (SUCCEEDED(hr)) {
        void* mappedData;
        m_lightBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, m_lightData.data(), lightBufferSize);
        m_lightBuffer->Unmap(0, nullptr);

        char debugMsg[256];
        sprintf_s(debugMsg, "Light buffer created: %zu lights, size: %u bytes\n",
            m_lightData.size(), lightBufferSize);
        OutputDebugStringA(debugMsg);
    } else {
        OutputDebugStringA("Failed to create light buffer\n");
    }
}

void DXRRenderer::CreateShaderTables() {
    ComPtr<ID3D12StateObjectProperties> stateObjectProps;
    HRESULT hr = m_rtStateObject.As(&stateObjectProps);
    if ( FAILED(hr) ) {
        throw std::runtime_error("Failed to get state object properties");
    }

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

    auto& gameManager = Singleton<GameManager>::getInstance();
    DXRScene* dxrScene = dynamic_cast<DXRScene*>( gameManager.GetScene().get() );
    if ( !dxrScene ) {
        throw std::runtime_error("No DXRScene found for shader table creation");
    }

    TLASData tlasData = dxrScene->GetTLASData();
    size_t numInstances = tlasData.blasDataList.size();

    void* lambertianHitGroupID = stateObjectProps->GetShaderIdentifier(L"HitGroup_Lambertian");
    void* metalHitGroupID = stateObjectProps->GetShaderIdentifier(L"HitGroup_Metal");
    void* dielectricHitGroupID = stateObjectProps->GetShaderIdentifier(L"HitGroup_Dielectric");
    void* lightHitGroupID = stateObjectProps->GetShaderIdentifier(L"HitGroup_DiffuseLight");

    void* materialShaderIDs[4] = {
        lambertianHitGroupID,
        metalHitGroupID,
        dielectricHitGroupID,
        lightHitGroupID
    };

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
}

// ReSTIR実行してないですが、リソースだけ作成してます
void DXRRenderer::CreateReSTIRResources() {
    char debugMsg[256];
    sprintf_s(debugMsg, "Creating ReSTIR resources for resolution %ux%u\n", m_width, m_height);
    OutputDebugStringA(debugMsg);

    UINT reservoirCount = m_width * m_height;
    UINT reservoirBufferSize = reservoirCount * sizeof(LightReservoir);

    static_assert(sizeof(::LightReservoir) == 52, "LightReservoir size must be 52 bytes");

    sprintf_s(debugMsg, "Reservoir buffer size: %u bytes (%u reservoirs)\n", reservoirBufferSize, reservoirCount);
    OutputDebugStringA(debugMsg);

    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);

    CD3DX12_RESOURCE_DESC currentReservoirDesc = CD3DX12_RESOURCE_DESC::Buffer(
        reservoirBufferSize,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    );

    HRESULT hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &currentReservoirDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_currentReservoirs)
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create Current Reservoir buffer");
    }

    m_currentReservoirs->SetName(L"Current ReSTIR Reservoirs");

    hr = m_device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &currentReservoirDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_previousReservoirs)
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create Previous Reservoir buffer");
    }

    m_previousReservoirs->SetName(L"Previous ReSTIR Reservoirs");

    CD3DX12_CPU_DESCRIPTOR_HANDLE currentHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        7,
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    );

    CD3DX12_CPU_DESCRIPTOR_HANDLE previousHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        8,
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    );

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = reservoirCount;
    uavDesc.Buffer.StructureByteStride = sizeof(::LightReservoir);

    m_device->CreateUnorderedAccessView(
        m_currentReservoirs.Get(),
        nullptr,
        &uavDesc,
        currentHandle
    );

    m_device->CreateUnorderedAccessView(
        m_previousReservoirs.Get(),
        nullptr,
        &uavDesc,
        previousHandle
    );

    OutputDebugStringA("ReSTIR resources created successfully\n");
}

void DXRRenderer::InitializeReSTIRBuffers() {
    if (m_restirInitialized) return;

    UINT reservoirCount = m_width * m_height;
    UINT bufferSize = reservoirCount * sizeof(::LightReservoir);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    HRESULT hr = m_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_restirUploadBuffer)
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create ReSTIR upload buffer");
    }

    void* mappedData;
    hr = m_restirUploadBuffer->Map(0, nullptr, &mappedData);
    if (SUCCEEDED(hr)) {
        memset(mappedData, 0, bufferSize);
        m_restirUploadBuffer->Unmap(0, nullptr);
    }

    CD3DX12_RESOURCE_BARRIER copyBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_currentReservoirs.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_DEST
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_previousReservoirs.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_DEST
        )
    };

    m_commandList->ResourceBarrier(2, copyBarriers);

    m_commandList->CopyResource(m_currentReservoirs.Get(), m_restirUploadBuffer.Get());
    m_commandList->CopyResource(m_previousReservoirs.Get(), m_restirUploadBuffer.Get());

    CD3DX12_RESOURCE_BARRIER uavBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_currentReservoirs.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_previousReservoirs.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        )
    };

    m_commandList->ResourceBarrier(2, uavBarriers);

    m_restirInitialized = true;
    OutputDebugStringA("ReSTIR buffers initialized\n");
}
