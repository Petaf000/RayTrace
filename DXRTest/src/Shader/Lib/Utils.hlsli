#ifndef UTILS_HLSLI
#define UTILS_HLSLI

#include "../Core/Types.hlsli"

// ===== 数学定数 =====
static const float PI = 3.14159265359f;
static const float INV_PI = 0.31830988618f;
static const float TWO_PI = 6.28318530718f;

// ===== 乱数生成 (PCG Hash) =====
uint PCGHash(uint seed)
{
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float RandomFloat(inout uint seed)
{
    seed = PCGHash(seed);
    return float(seed) / 4294967295.0f;
}

// ===== ベクトル・計算ヘルパー =====

// 法線を 0-1 のRGBカラーに変換（デバッグ用）
float3 NormalToColor(float3 normal)
{
    return normal * 0.5f + 0.5f;
}

// シャドウアクネ防止のためにレイの開始点を少し浮かす
float3 OffsetRay(const float3 p, const float3 n)
{
    // 0.001f はシーンのスケールに合わせて調整
    return p + n * 0.001f;
}

// 球面上のランダムな方向を返す
float3 RandomUnitVector(inout uint seed)
{
    float z = RandomFloat(seed) * 2.0f - 1.0f;
    float a = RandomFloat(seed) * TWO_PI;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

// コサイン重点サンプリング方向
float3 RandomCosineDirection(inout uint seed)
{
    float r1 = RandomFloat(seed);
    float r2 = RandomFloat(seed);
    float z = sqrt(1.0f - r2);
    float phi = TWO_PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    return float3(x, y, z);
}

// ===== G-Buffer 設定ヘルパー =====
// ClosestHitシェーダーで共通して使う処理
void SetGBufferData(inout RayPayload payload, float3 worldPos, float3 worldNormal,
                   float3 albedo, uint materialType, float roughness, float hitDistance)
{
    // 最初のヒット（直接カメラから見えた場所）のみ記録
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