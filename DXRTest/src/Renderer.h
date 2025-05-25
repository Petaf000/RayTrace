#pragma once


struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};

class Scene;
class Material;

class Renderer {
public:
    void Init();
    void Cleanup();
    void Update();
    void Render();

	void WaitGPU() {
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));

        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

        m_fenceValue++;
	}


	ComPtr<ID3D12Device> GetDevice() const { return m_device; }
	ComPtr<ID3D12Device5> GetDevice5() const { return m_device5; }

	

private:
	friend class Singleton<Renderer>;

    Renderer();
    ~Renderer();


    void LoadPipeline();
    void LoadAssets();

    static const uint32_t m_frameBufferCount = 2;

    // DirectX 12 �I�u�W�F�N�g
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Resource> m_renderTargets[m_frameBufferCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    uint32_t m_rtvDescriptorSize;
    uint32_t m_frameIndex;

    // �����p�I�u�W�F�N�g
    ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue;
    HANDLE m_fenceEvent;

    // �p�C�v���C���X�e�[�g
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // ���_�o�b�t�@
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // �E�B���h�E�̃T�C�Y
    uint32_t m_bufferWidth;
    uint32_t m_bufferHeight;
	HWND m_hWnd;



    // DXR�p�ϐ�
    ComPtr<ID3D12Device5> m_device5;
    ComPtr<ID3D12GraphicsCommandList4> m_commandList4;
    ComPtr<ID3D12RootSignature> m_dxrGlobalRootSignature;
    //ComPtr<ID3D12Resource> m_bottomLevelAS;
    ComPtr<ID3D12Resource> m_topLevelAS;
    ComPtr<ID3D12Resource> m_dxrOutput;
    ComPtr<ID3D12DescriptorHeap> m_dxrDescriptorHeap;
    UINT m_dxrDescriptorSize;

    // BLAS�p
    std::vector<ComPtr<ID3D12Resource>> m_bottomLevelASs;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> m_instanceDescs;

    // �V�F�[�_�[�e�[�u��
    ComPtr<ID3D12Resource> m_shaderTable;
    UINT m_shaderTableEntrySize;
    UINT m_shaderRecordSize;
    UINT m_rayGenSectionSize;
    UINT m_missSectionSize;
    UINT m_hitGroupSectionSize;
    UINT64 m_rayGenShaderTableGpuAddr;
    UINT64 m_missShaderTableGpuAddr;
    UINT64 m_hitGroupShaderTableGpuAddr;

    // ���C�g���[�V���OPSO
    ComPtr<ID3D12StateObject> m_rtStateObject;
    ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;

    // �}�e���A���萔�o�b�t�@
    struct MaterialConstantBuffer {
        XMFLOAT4 baseColor;
        XMFLOAT4 emissiveColor;
        float metallic;
        float roughness;
        float ior;
        float padding;
    };
    ComPtr<ID3D12Resource> m_materialConstantBuffer;
    MaterialConstantBuffer* m_materialConstantBufferData = nullptr;


    // �o�b�t�@
    ComPtr<ID3D12Resource> m_vertexBuffer2;   // ���C�g���[�V���O�p�̕ʒ��_�o�b�t�@
    ComPtr<ID3D12Resource> m_colorBuffer;     // �F���p�o�b�t�@

    // DXR�p�֐�
    void CreateRaytracingInterfaces();
    void CreateRaytracingRootSignature();
    void CreateRaytracingPipelineState();
    void CreateRaytracingDescriptorHeap();
    void CreateRaytracingOutputResource();
    void CreateMaterialConstantBuffers();
    void UpdateMaterialData(UINT materialIndex, const Material* material);
    // Must be called after OpenScene
    void CreateSceneAccelerationStructures();

    // Must be called after CreateSceneAccelerationStructures
    void CreateRaytracingShaderBindingTable();

    ComPtr<ID3D12Resource> CreateBuffer(
        UINT64 size,
        D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES initialState,
        const D3D12_HEAP_PROPERTIES& heapProps);

    // �R���p�C���ς݃V�F�[�_�[��ǂݍ��ރ��\�b�h
    void LoadCompiledShader(const wchar_t* filename, ComPtr<ID3DBlob>& shaderBlob);

    // �R���p�C���ς݃V�F�[�_�[�u���u
    ComPtr<ID3DBlob> m_rayGenShaderBlob;
    ComPtr<ID3DBlob> m_missShaderBlob;
    ComPtr<ID3DBlob> m_hitShaderBlob;

	// ���C�g���ύX�p�ϐ�
    bool m_raytracing = false;


};
