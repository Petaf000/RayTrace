#pragma once
struct DXRVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texCoord;
};

struct DXRMaterialData {
    XMFLOAT3 albedo;
    float roughness;
    float refractiveIndex;
    XMFLOAT3 emission;
    int materialType;
    float padding[1];
};

struct BLASData {
    ComPtr<ID3D12Resource> vertexBuffer;
    std::vector<DXRVertex> vertices;
    std::vector<uint32_t> indices;
    ComPtr<ID3D12Resource> indexBuffer;
    uint32_t materialID;
    XMMATRIX transform;
    ComPtr<ID3D12Resource> scratchBuffer;
};

struct TLASData {
    std::vector<BLASData> blasDataList;
    std::vector<XMMATRIX> instanceTransforms;
    ComPtr<ID3D12Resource> instanceBuffer;
};

struct DXRLightData {
    XMFLOAT3 position;
    float padding1;
    XMFLOAT3 emission;
    float padding2;
    XMFLOAT3 size;
    float area;
    XMFLOAT3 normal;
    uint32_t lightType;
    uint32_t instanceID;
    float lightPadding[3];
};

struct LightReservoir {
    uint32_t lightIndex;
    XMFLOAT3 samplePos;
    XMFLOAT3 radiance;
    float weight;
    uint32_t sampleCount;
    float weightSum;
    float pdf;
    uint32_t valid;
    float padding;
};

struct PathVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT3 albedo;
    XMFLOAT3 emission;
    uint32_t materialType;
    float roughness;
    XMFLOAT2 padding;
};

struct GIPath {
    PathVertex vertices[4];
    XMFLOAT3 throughput;
    XMFLOAT3 radiance;
    uint32_t vertexCount;
    float pathPdf;
    float misWeight;
    uint32_t valid;
};

struct GIReservoir {
    GIPath selectedPath;
    float weight;
    uint32_t sampleCount;
    float weightSum;
    float pathPdf;
    uint32_t valid;
    XMFLOAT3 padding;
};

namespace DXRConfig {
    static constexpr UINT MAX_INSTANCES = 128;
    static constexpr UINT MAX_MATERIALS = 64;
    static constexpr UINT MAX_LIGHTS = 32;
    static constexpr UINT MAX_RAY_DEPTH = 8;
    static constexpr UINT SHADER_IDENTIFIER_SIZE = 32;
}