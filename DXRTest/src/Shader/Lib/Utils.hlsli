#ifndef UTILS_HLSLI
#define UTILS_HLSLI

#include "../Core/Types.hlsli"

// ===============================================================================================
//  定数・データ
// ===============================================================================================

static const float PI = 3.14159265359f;
static const float INV_PI = 0.31830988618f;
static const float TWO_PI = 6.28318530718f;

// ブルーノイズパターン (8x8)
static const float BlueNoise8x8[64] =
{
    0.515625f, 0.140625f, 0.890625f, 0.328125f, 0.484375f, 0.171875f, 0.921875f, 0.359375f,
    0.015625f, 0.765625f, 0.265625f, 0.703125f, 0.046875f, 0.796875f, 0.296875f, 0.734375f,
    0.640625f, 0.078125f, 0.828125f, 0.203125f, 0.671875f, 0.109375f, 0.859375f, 0.234375f,
    0.390625f, 0.953125f, 0.453125f, 0.578125f, 0.421875f, 0.984375f, 0.484375f, 0.609375f,
    0.546875f, 0.125000f, 0.859375f, 0.281250f, 0.515625f, 0.156250f, 0.890625f, 0.312500f,
    0.078125f, 0.734375f, 0.234375f, 0.656250f, 0.109375f, 0.765625f, 0.265625f, 0.687500f,
    0.703125f, 0.031250f, 0.796875f, 0.156250f, 0.734375f, 0.062500f, 0.828125f, 0.187500f,
    0.343750f, 0.906250f, 0.406250f, 0.531250f, 0.375000f, 0.937500f, 0.437500f, 0.562500f
};

// ===============================================================================================
//  乱数生成
// ===============================================================================================

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

// ブルーノイズシード生成
uint GenerateBlueNoiseSeed(uint2 pixelCoord, uint frameIndex, uint sampleIndex)
{
    uint2 noiseCoord = pixelCoord & 7;
    uint noiseIndex = noiseCoord.y * 8 + noiseCoord.x;
    float blueNoiseValue = BlueNoise8x8[noiseIndex];
    
    uint blueNoiseSeed = uint(blueNoiseValue * 4294967295.0f);
    uint seed = blueNoiseSeed;
    seed ^= PCGHash(pixelCoord.x * 73856093u);
    seed ^= PCGHash(pixelCoord.y * 19349663u);
    seed ^= PCGHash(frameIndex * 83492791u);
    seed ^= PCGHash(sampleIndex * 51726139u);
    return PCGHash(seed);
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

// ===============================================================================================
//  物理ベース計算・ベクトル演算 (Fresnel, Reflect, Refract)
// ===============================================================================================

// 法線を 0-1 のRGBカラーに変換（デバッグ用）
float3 NormalToColor(float3 normal)
{
    return normal * 0.5f + 0.5f;
}

// シャドウアクネ防止のためにレイの開始点を少し浮かす
float3 OffsetRay(const float3 p, const float3 n)
{
    return p + n * 0.001f;
}

// 反射ベクトル計算
float3 ReflectVec(float3 v, float3 n)
{
    return v - 2.0f * dot(v, n) * n;
}

// 屈折ベクトル計算 (戻り値: 屈折可能かどうか)
bool RefractVec(float3 v, float3 n, float ni_over_nt, out float3 refracted)
{
    float3 uv = normalize(v);
    float dt = dot(uv, n);
    float D = 1.0f - (ni_over_nt * ni_over_nt) * (1.0f - dt * dt);
    
    if (D > 0.0f)
    {
        refracted = -ni_over_nt * (uv - n * dt) - n * sqrt(D);
        return true;
    }
    return false;
}

// Schlick近似によるフレネル反射率
float SchlickFresnel(float cosine, float ri)
{
    float r0 = (1.0f - ri) / (1.0f + ri);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

// ===============================================================================================
//  色空間変換・トーンマッピング
// ===============================================================================================

// ACES Film Tone Mapping Approximation
float3 ACESToneMapping(float3 color)
{
    const float3 input = color * 0.6f;
    const float3 a = input * (input + 0.0245786f) - 0.000090537f;
    const float3 b = input * (0.983729f * input + 0.4329510f) + 0.238081f;
    return a / b;
}

// sRGB Gamma Correction
float3 LinearToSRGB(float3 color)
{
    float3 srgb;
    srgb.x = (color.x <= 0.0031308f) ? (color.x * 12.92f) : (1.055f * pow(color.x, 1.0f / 2.4f) - 0.055f);
    srgb.y = (color.y <= 0.0031308f) ? (color.y * 12.92f) : (1.055f * pow(color.y, 1.0f / 2.4f) - 0.055f);
    srgb.z = (color.z <= 0.0031308f) ? (color.z * 12.92f) : (1.055f * pow(color.z, 1.0f / 2.4f) - 0.055f);
    return srgb;
}

// 特定のカラーマトリックス変換 (色味調整)
float3 ApplyColorMatrix(float3 color)
{
    const float3x3 colorMatrix = float3x3(
        1.0478112f, 0.0228866f, -0.0501270f,
        -0.0295081f, 0.9904844f, 0.0150436f,
        -0.0092345f, 0.0150436f, 0.7521316f
    );
    return mul(colorMatrix, color);
}

// ===============================================================================================
//  G-Buffer 設定ヘルパー
// ===============================================================================================

void SetGBufferData(inout RayPayload payload, float3 worldPos, float3 worldNormal,
                   float3 albedo, uint materialType, float roughness, float hitDistance)
{
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