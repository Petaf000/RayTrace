// shader/Common.hlsli - 共通ヘッダー
#ifndef COMMON_HLSLI
#define COMMON_HLSLI


// 定数バッファ
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4 lightPosition;
    float4 lightColor;
};

// グローバルリソース
RaytracingAccelerationStructure SceneBVH : register(t0);
RWTexture2D<float4> RenderTarget : register(u0);

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
    float2 barycentrics: SV_IntersectionAttributes;
};

// マテリアルタイプ
#define MATERIAL_LAMBERTIAN  0
#define MATERIAL_METAL       1
#define MATERIAL_DIELECTRIC  2
#define MATERIAL_LIGHT       3

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

// 数学的定数
#define PI 3.14159265359f
#define INV_PI 0.31830988618f

// ユーティリティ関数
float3 OffsetRay(const float3 p, const float3 n)
{
    return p + n * 0.001f;
}

// 乱数生成（簡易版）
uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

float RandomFloat(inout uint seed)
{
    seed = WangHash(seed);
    return (seed & 0xFFFFFF) / float(0x1000000);
}

float3 RandomUnitVector(inout uint seed)
{
    float z = RandomFloat(seed) * 2.0f - 1.0f;
    float a = RandomFloat(seed) * 2.0f * PI;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

float3 RandomCosineDirection(inout uint seed)
{
    float r1 = RandomFloat(seed);
    float r2 = RandomFloat(seed);
    
    float z = sqrt(1.0f - r2);
    float phi = 2.0f * PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    
    return float3(x, y, z);
}

#endif // COMMON_HLSLI