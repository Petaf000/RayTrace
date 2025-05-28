#pragma once
// DXR�p�̊g�����_�\����
struct DXRVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texCoord;
};

// DXR�p�}�e���A���f�[�^
struct DXRMaterialData {
    XMFLOAT3 albedo;
    float roughness;        // Metal�p
    float refractiveIndex;  // Dielectric�p  
    XMFLOAT3 emission;      // DiffuseLight�p
    int materialType;       // 0=Lambertian, 1=Metal, 2=Dielectric, 3=Light
    float padding[1];       // �A���C�����g����
};

// BLAS�\�z�p�f�[�^
struct BLASData {
    ComPtr<ID3D12Resource> vertexBuffer;
    std::vector<DXRVertex> vertices;
    std::vector<uint32_t> indices;
    ComPtr<ID3D12Resource> indexBuffer;
    DXRMaterialData material;
    XMMATRIX transform;
    ComPtr<ID3D12Resource> scratchBuffer;
};

// TLAS�\�z�p�f�[�^  
struct TLASData {
    std::vector<BLASData> blasDataList;
    std::vector<XMMATRIX> instanceTransforms;
    ComPtr<ID3D12Resource> instanceBuffer;
};