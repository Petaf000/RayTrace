#pragma once

// DXR�p�̍\���̒�`�iCPU���C�g���Ɠ����\���j
struct DXRVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texcoord;
};

struct DXRMaterial {
    XMFLOAT3 albedo;
    int materialType; // 0: Lambertian, 1: Metal, 2: Dielectric, 3: Light
    float roughness;
    float refractiveIndex;
    XMFLOAT3 emission;
    float padding;
};

struct DXRInstance {
    XMMATRIX worldMatrix;
    int materialIndex;
    int geometryIndex;
    XMFLOAT2 padding;
};

struct DXRCameraData {
    XMFLOAT3 position;
    float fov;
    XMFLOAT3 forward;
    float aspect;
    XMFLOAT3 up;
    float padding;
    XMFLOAT3 right;
    float padding2;
};

struct DXRGlobalConstants {
    DXRCameraData camera;
    XMFLOAT4 backgroundColor;
    int maxDepth;
    int sampleCount;
    int frameIndex;
    int padding;
};

class DXRRenderer {
public:
    DXRRenderer();
    ~DXRRenderer();

    bool Init(ID3D12Device5* device, ID3D12CommandQueue* commandQueue, HWND hwnd, int width, int height);
    void UnInit();
    void Render();
    void ResizeBuffers(int width, int height);

    // �V�[���f�[�^�ݒ�
    void SetMaterials(const std::vector<DXRMaterial>& materials);
    void SetGeometries(const std::vector<std::vector<DXRVertex>>& vertices,
        const std::vector<std::vector<uint32_t>>& indices);
    void SetInstances(const std::vector<DXRInstance>& instances);
    void SetCamera(const DXRCameraData& camera);
    void SetGlobalConstants(const DXRGlobalConstants& constants);
    void CreateVertexIndexSRVs();

    // BLAS/TLAS�\�z
    void BuildBLAS();
    void BuildTLAS();

    void WaitForGPU();

private:
    // DXR������
    bool InitDXR();
    bool LoadShaders();
    bool CreateRootSignatures();
    bool CreateRaytracingPipeline();
    bool CreateDescriptorHeaps();
    bool CreateOutputResource();
    bool CreateShaderBindingTable();

    // �A�N�Z�����[�V�����\���쐬
    bool CreateBottomLevelAS();
    bool CreateTopLevelAS();

    // ���\�[�X�쐬�w���p�[
    ComPtr<ID3D12Resource> CreateBuffer(UINT64 size, D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES initState,
        const D3D12_HEAP_PROPERTIES& heapProps);
    UINT64 AlignUp(UINT64 value, UINT64 alignment);
    bool CreateGeometryBuffers(size_t geometryIndex,
        const std::vector<DXRVertex>& vertices,
        const std::vector<uint32_t>& indices);
    bool UploadBufferData(ID3D12Resource* destBuffer,
        const void* data, UINT dataSize);

private:
    // DXR�f�o�C�X
    ComPtr<ID3D12Device5> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList4> m_commandList;

    // DXR�p�C�v���C��
    ComPtr<ID3D12RootSignature> m_globalRootSignature;
    ComPtr<ID3D12RootSignature> m_localRootSignature;
    ComPtr<ID3D12StateObject> m_raytracingPipeline;
    ComPtr<ID3D12StateObjectProperties> m_raytracingPipelineProperties;

    // �V�F�[�_�[
    ComPtr<ID3DBlob> m_raygenShader;
    ComPtr<ID3DBlob> m_missShader;
    ComPtr<ID3DBlob> m_closestHitShader;

    // �o�̓��\�[�X
    ComPtr<ID3D12Resource> m_outputResource;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;

    // �A�N�Z�����[�V�����\��
    std::vector<ComPtr<ID3D12Resource>> m_bottomLevelAS;
    ComPtr<ID3D12Resource> m_topLevelAS;
    ComPtr<ID3D12Resource> m_instanceBuffer;

    // �W�I���g���o�b�t�@
    std::vector<ComPtr<ID3D12Resource>> m_vertexBuffers;
    std::vector<ComPtr<ID3D12Resource>> m_indexBuffers;

    // �V�F�[�_�[�o�C���f�B���O�e�[�u��
    ComPtr<ID3D12Resource> m_shaderBindingTable;

    // �萔�o�b�t�@
    ComPtr<ID3D12Resource> m_globalConstantBuffer;
    ComPtr<ID3D12Resource> m_materialBuffer;

    // �V�[���f�[�^
    std::vector<DXRMaterial> m_materials;
    std::vector<std::vector<DXRVertex>> m_geometryVertices;
    std::vector<std::vector<uint32_t>> m_geometryIndices;
    std::vector<DXRInstance> m_instances;
    DXRCameraData m_camera;
    DXRGlobalConstants m_globalConstants;

    // �����_�[�^�[�Q�b�g
    std::vector<ComPtr<ID3D12Resource>> m_renderTargets;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;

    // �t�F���X
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;

    // �E�B���h�E���
    HWND m_hwnd;
    int m_width;
    int m_height;
    int m_frameIndex;

    // DXR�T�|�[�g
    bool m_dxrSupported;
};