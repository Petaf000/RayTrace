#pragma once
// DXR用の拡張頂点構造体
struct DXRVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texCoord;
};

// DXR用マテリアルデータ
struct DXRMaterialData {
    XMFLOAT3 albedo;
    float roughness;        // Metal用
    float refractiveIndex;  // Dielectric用  
    XMFLOAT3 emission;      // DiffuseLight用
    int materialType;       // 0=Lambertian, 1=Metal, 2=Dielectric, 3=Light
    float padding[1];       // アライメント調整
};

// BLAS構築用データ
struct BLASData {
    ComPtr<ID3D12Resource> vertexBuffer;
    std::vector<DXRVertex> vertices;
    std::vector<uint32_t> indices;
    ComPtr<ID3D12Resource> indexBuffer;
    DXRMaterialData material;
    XMMATRIX transform;
    ComPtr<ID3D12Resource> scratchBuffer;
};

// TLAS構築用データ  
struct TLASData {
    std::vector<BLASData> blasDataList;
    std::vector<XMMATRIX> instanceTransforms;
    ComPtr<ID3D12Resource> instanceBuffer;
};