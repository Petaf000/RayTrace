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

// ★★★ DXR設定定数 ★★★
namespace DXRConfig {
    static constexpr UINT MAX_INSTANCES = 128;           // 最大インスタンス数
    static constexpr UINT MAX_MATERIALS = 64;            // 最大マテリアル数
    static constexpr UINT MAX_RAY_DEPTH = 8;             // 最大レイの深度
    static constexpr UINT SHADER_IDENTIFIER_SIZE = 32;   // シェーダー識別子サイズ
}