#pragma once

#include "DXRData.h"

class Renderer;

class DXRRenderer {
public:
    void Init(Renderer* renderer);
    void UnInit();
    void Render();
	void RenderDXRIMGUI();

	UINT GetWidth() const { return m_width; }
	UINT GetHeight() const { return m_height; }
	
	// ATrousDenoiserÇÕåªç›égÇ¡ÇƒÇ‹ÇπÇÒ(TemporalAccumulationÇÃÇ›é¿çsÇ≈Ç´Ç‹Ç∑)
    /*void SetDenoiserEnabled(bool enabled) { m_denoiserEnabled = enabled; }
    void SetDenoiserIterations(int iterations) { m_denoiserIterations = max(1, min(5, iterations)); }
    void SetDenoiserParameters(float colorSigma, float normalSigma, float depthSigma) {
        m_colorSigma = colorSigma;
        m_normalSigma = normalSigma;
        m_depthSigma = depthSigma;
    }*/
private:
    friend class Singleton<DXRRenderer>;

    DXRRenderer() = default;
    ~DXRRenderer() = default;

    void InitializeDXR(ID3D12Device* device);
    void CreateRootSignature();
    void CreateRaytracingPipelineStateObject();
    void CreateShaderTables();
    void CreateOutputResource();
    void CreateMaterialBuffer(const TLASData& tlasData);
    void CreateLightBuffer(const TLASData& tlasData);
    void CollectLightsFromScene(const TLASData& tlasData);

    void CreateAccelerationStructures();
    void CreateBLAS(BLASData& blasData, ComPtr<ID3D12Resource>& blasBuffer);
    void CreateTLAS(TLASData& tlasData);
    void CreateVertexIndexBuffers(const TLASData& tlasData);
    void CreateInstanceOffsetBuffer(const TLASData& tlasData, const std::vector<uint32_t>& vertexOffsets, const std::vector<uint32_t>& indexOffsets);

    void CreateDebugBufferViews();

    void CreateDenoiserResources();
    void CreateDenoiserPipeline();
    ComPtr<ID3D12Resource> RunDenoiser();
    void UpdateDenoiserConstants(int stepSize);
    
    void CreateReSTIRResources();
    void InitializeReSTIRBuffers();

    ComPtr<IDxcBlob> CompileShaderFromFile(const std::wstring& hlslPath,
        const std::wstring& entryPoint,
        const std::wstring& target = L"lib_6_5");

    ComPtr<IDxcBlob> LoadOrCompileShader(const std::wstring& hlslPath,
        const std::wstring& entryPoint,
        const std::wstring& target = L"lib_6_5");

    ComPtr<IDxcBlob> LoadCSO(const std::wstring& filename);

    void UpdateCamera();
    UINT AlignTo(UINT size, UINT alignment);



    ComPtr<ID3D12Device5> m_device;
    ComPtr<ID3D12GraphicsCommandList4> m_commandList;
    ComPtr<ID3D12CommandQueue> m_commandQueue;

    ComPtr<ID3D12RootSignature> m_globalRootSignature;
    ComPtr<ID3D12StateObject> m_rtStateObject;

    ComPtr<ID3D12Resource> m_topLevelAS;
    ComPtr<ID3D12Resource> m_topLevelASScratch;
    std::vector<ComPtr<ID3D12Resource>> m_bottomLevelAS;
    std::vector<ComPtr<ID3D12Resource>> m_bottomLevelASScratch;

    ComPtr<ID3D12Resource> m_rayGenShaderTable;
    ComPtr<ID3D12Resource> m_missShaderTable;
    ComPtr<ID3D12Resource> m_hitGroupShaderTable;

    ComPtr<IDxcUtils> m_dxcUtils;
    ComPtr<IDxcCompiler3> m_dxcCompiler;
    ComPtr<IDxcIncludeHandler> m_includeHandler;

    ComPtr<ID3D12Resource> m_raytracingOutput;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;

    ComPtr<ID3D12PipelineState> m_denoiserPSO;
    ComPtr<ID3D12RootSignature> m_denoiserRootSignature;
    ComPtr<ID3D12Resource> m_denoiserConstants;
    ComPtr<ID3D12DescriptorHeap> m_denoiserDescriptorHeap;
    ComPtr<ID3D12Resource> m_albedoBuffer;
    ComPtr<ID3D12Resource> m_normalBuffer;
    ComPtr<ID3D12Resource> m_depthBuffer;
    ComPtr<ID3D12Resource> m_denoisedOutput;
    
    ComPtr<ID3D12Resource> m_accumulationBuffer;
    ComPtr<ID3D12Resource> m_prevFrameDataBuffer;
    bool m_temporalAccumulationInitialized = false;
    
    ComPtr<ID3D12Resource> m_currentReservoirs;
    ComPtr<ID3D12Resource> m_previousReservoirs;
    ComPtr<ID3D12Resource> m_restirUploadBuffer;
    bool m_restirInitialized = false;
    
    ComPtr<ID3D12Resource> m_currentGIReservoirs;
    ComPtr<ID3D12Resource> m_previousGIReservoirs;
    ComPtr<ID3D12Resource> m_giUploadBuffer;
    bool m_restirGIInitialized = false;

    struct DenoiserConstants {
        int stepSize;
        float colorSigma;
        float normalSigma;
        float depthSigma;
        DirectX::XMFLOAT2 texelSize;
        DirectX::XMFLOAT2 padding;
    };

    struct SceneConstantBuffer {
        XMMATRIX projectionMatrix;
        XMMATRIX viewMatrix;

        DirectX::XMFLOAT3 cameraRight;
        float tanHalfFov;

        DirectX::XMFLOAT3 cameraUp;
        float aspectRatio;

        DirectX::XMFLOAT3 cameraForward;
        float frameCount;
        
        uint32_t numLights;
        uint32_t cameraMovedFlag;
        float padding[2];
    };
    ComPtr<ID3D12Resource> m_sceneConstantBuffer;

    ComPtr<ID3D12Resource> m_materialBuffer;
    ComPtr<ID3D12Resource> m_globalVertexBuffer;
    ComPtr<ID3D12Resource> m_globalIndexBuffer;
    ComPtr<ID3D12Resource> m_instanceOffsetBuffer;
    
    ComPtr<ID3D12Resource> m_lightBuffer;
    std::vector<DXRLightData> m_lightData;
    uint32_t m_numLights = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE m_debugSrvHeapStart_CPU;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_debugSrvHeapStart_GPU;

    UINT m_totalVertexCount = 0;
    UINT m_totalIndexCount = 0;

    static const UINT s_shaderIdentifierSize = 32;
    static const UINT s_shaderTableEntrySize = 32;
    UINT s_hitGroupEntrySize = 32;

    UINT m_width = 1920;
    UINT m_height = 1080;

    bool m_denoiserEnabled = false;
    int m_denoiserIterations = 3;
    float m_colorSigma = 0.125f;
    float m_normalSigma = 32.0f;
    float m_depthSigma = 0.1f;
};