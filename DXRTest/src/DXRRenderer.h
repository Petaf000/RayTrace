// DXRRenderer.h
#pragma once

#include "DXRData.h"

class Renderer;

class DXRRenderer {
public:
    void Init(Renderer* renderer);
    void UnInit();
    void Render();

	UINT GetWidth() const { return m_width; }
	UINT GetHeight() const { return m_height; }
	
private:
    friend class Singleton<DXRRenderer>;

    DXRRenderer() = default;
    ~DXRRenderer() = default;

    // DXR������
    void InitializeDXR(ID3D12Device* device);
    void CreateRootSignature();
    void CreateRaytracingPipelineStateObject();
    void CreateShaderTables();
    void CreateOutputResource();
    void CreateLocalRootSignature();
    void CreateMaterialConstantBuffers();
    void UpdateMaterialData();

    // ���\�[�X�쐬
    void CreateAccelerationStructures();
    void CreateBLAS(BLASData& blasData, ComPtr<ID3D12Resource>& blasBuffer);
    void CreateTLAS(TLASData& tlasData);

    // DXCCompiler
    // ���s��HLSL�R���p�C���֐�
    ComPtr<IDxcBlob> CompileShaderFromFile(const std::wstring& hlslPath,
        const std::wstring& entryPoint,
        const std::wstring& target = L"lib_6_5");

    // �L���b�V���@�\�t�����[�h�֐�
    ComPtr<IDxcBlob> LoadOrCompileShader(const std::wstring& hlslPath,
        const std::wstring& entryPoint,
        const std::wstring& target = L"lib_6_5");

    // �V�F�[�_�[�֘A
    ComPtr<IDxcBlob> LoadCSO(const std::wstring& filename);

    // �w���p�[�֐�
    void UpdateCamera();
    UINT AlignTo(UINT size, UINT alignment);

    // DXR�֘A�I�u�W�F�N�g
    ComPtr<ID3D12Device5> m_device;
    ComPtr<ID3D12GraphicsCommandList4> m_commandList;
    ComPtr<ID3D12CommandQueue> m_commandQueue;

    // ���[�g�V�O�l�`���ƃp�C�v���C��
    ComPtr<ID3D12RootSignature> m_globalRootSignature;
    ComPtr<ID3D12StateObject> m_rtStateObject;
    // ���[�J�����[�g�V�O�l�`���ǉ�
    ComPtr<ID3D12RootSignature> m_localRootSignature;

    // �A�N�Z�����[�V�����\��
    ComPtr<ID3D12Resource> m_topLevelAS;
    ComPtr<ID3D12Resource> m_topLevelASScratch;
    std::vector<ComPtr<ID3D12Resource>> m_bottomLevelAS;
    std::vector<ComPtr<ID3D12Resource>> m_bottomLevelASScratch;

    // �V�F�[�_�[�e�[�u��
    ComPtr<ID3D12Resource> m_rayGenShaderTable;
    ComPtr<ID3D12Resource> m_missShaderTable;
    ComPtr<ID3D12Resource> m_hitGroupShaderTable;

    // DXC�R���p�C���[�C���X�^���X
    ComPtr<IDxcUtils> m_dxcUtils;
    ComPtr<IDxcCompiler3> m_dxcCompiler;
    ComPtr<IDxcIncludeHandler> m_includeHandler;

    // �o�̓��\�[�X
    ComPtr<ID3D12Resource> m_raytracingOutput;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;

    // �萔�o�b�t�@
    struct SceneConstantBuffer {
        XMMATRIX viewProjectionMatrix;
        XMFLOAT4 cameraPosition;
        XMFLOAT4 lightPosition;
        XMFLOAT4 lightColor;
    };
    ComPtr<ID3D12Resource> m_sceneConstantBuffer;

    // �}�e���A���萔�o�b�t�@
    std::vector<ComPtr<ID3D12Resource>> m_materialConstantBuffers;

    // �V�F�[�_�[���ʎq
    static const UINT s_shaderIdentifierSize = 32;
    static const UINT s_shaderTableEntrySize = 32;
    UINT s_hitGroupEntrySize = 32; // ���I�ɐݒ�

    // �f�B�����V����
    UINT m_width = 800;
    UINT m_height = 600;
};