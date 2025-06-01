#ifndef GEOMETRYDATA_HLSLI
#define GEOMETRYDATA_HLSLI

// マテリアルタイプ
#define MATERIAL_LAMBERTIAN  0
#define MATERIAL_METAL       1
#define MATERIAL_DIELECTRIC  2
#define MATERIAL_LIGHT       3


// 頂点データ構造体
struct DXRVertex
{
    float3 position;
    float3 normal;
    float2 texCoord;
};

// レイペイロード
struct RayPayload
{
    float3 color;
    uint depth;
    uint seed;
};

// 頂点属性
struct VertexAttributes
{
    float2 barycentrics : SV_IntersectionAttributes;
};

// マテリアルデータ（シェーダー用）
struct MaterialData
{
    float3 albedo;
    float roughness;
    float refractiveIndex;
    float3 emission;
    int materialType;
    float padding;
};

// インスタンスオフセット情報
struct InstanceOffsetData
{
    uint vertexOffset;
    uint indexOffset;
    uint2 padding;
};

#endif