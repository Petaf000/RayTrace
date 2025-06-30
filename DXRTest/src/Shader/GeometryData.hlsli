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

// 拡張されたレイペイロード
struct RayPayload
{
    float3 color;
    uint depth;
    uint seed;
    
    // G-Buffer用データ (プライマリレイの場合のみ使用)
    float3 albedo; // マテリアルのアルベド
    float3 normal; // ワールド空間法線
    float3 worldPos; // ワールド座標
    float hitDistance; // ヒット距離（深度用）
    uint materialType; // マテリアルタイプ
    float roughness; // ラフネス値
    uint padding; // アライメント用
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
    uint materialID;
    uint padding;
};

// G-Bufferデータを設定するヘルパー関数
void SetGBufferData(inout RayPayload payload, float3 worldPos, float3 worldNormal,
                   float3 albedo, uint materialType, float roughness, float hitDistance)
{
    // プライマリレイの場合のみG-Bufferデータを設定
    if (payload.depth == 0)
    {
        payload.worldPos = worldPos;
        payload.normal = worldNormal;
        payload.albedo = albedo;
        payload.materialType = materialType;
        payload.roughness = roughness;
        payload.hitDistance = hitDistance;
    }
}

#endif