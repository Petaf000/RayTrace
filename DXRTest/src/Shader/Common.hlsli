// shader/Common.hlsli - 共通ヘッダー（デバッグ版）
#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#include "Utils.hlsli"
#include "GeometryData.hlsli"
#include "LightFunc.hlsli"

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

// ★★★ 追加：各種バッファ ★★★
StructuredBuffer<MaterialData> MaterialBuffer : register(t1);
StructuredBuffer<DXRVertex> VertexBuffer : register(t2);
StructuredBuffer<uint> IndexBuffer : register(t3);
StructuredBuffer<InstanceOffsetData> InstanceOffsetBuffer : register(t4);

// ★★★ 追加：マテリアル取得ヘルパー関数 ★★★
MaterialData GetMaterial(uint instanceID)
{
    // 境界チェック付きでマテリアルを取得
    if (instanceID < 32) // 想定される最大インスタンス数
    {
        return MaterialBuffer[instanceID];
    }
    else
    {
        // デフォルトマテリアル
        MaterialData defaultMaterial;
        defaultMaterial.albedo = float3(1.0f, 0.0f, 1.0f); // マゼンタ（エラー色）
        defaultMaterial.roughness = 1.0f;
        defaultMaterial.refractiveIndex = 1.0f;
        defaultMaterial.emission = float3(0, 0, 0);
        defaultMaterial.materialType = MATERIAL_LAMBERTIAN;
        defaultMaterial.padding = 0.0f;
        return defaultMaterial;
    }
}

// ★★★ デバッグ用：法線を色に変換する関数 ★★★
float3 NormalToColor(float3 normal)
{
    // 法線ベクトル (-1 to 1) を色 (0 to 1) に変換
    return normal * 0.5f + 0.5f;
}

// ★★★ 最もシンプルな法線取得（デバッグ用）★★★
float3 GetInterpolatedNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    // 1. インスタンスのオフセット情報を取得
    InstanceOffsetData offsetData = InstanceOffsetBuffer[instanceID];
    
    // 2. プリミティブIDから三角形の3つのインデックスを取得
    uint indexOffset = offsetData.indexOffset + primitiveID * 3;
    uint i0 = IndexBuffer[indexOffset + 0] + offsetData.vertexOffset;
    uint i1 = IndexBuffer[indexOffset + 1] + offsetData.vertexOffset;
    uint i2 = IndexBuffer[indexOffset + 2] + offsetData.vertexOffset;
    
    // 3. 3つの頂点の法線を取得（ローカル座標）
    float3 n0 = VertexBuffer[i0].normal;
    float3 n1 = VertexBuffer[i1].normal;
    float3 n2 = VertexBuffer[i2].normal;
    
    // ★★★ デバッグ：頂点法線をそのまま返す（変換なし）★★★
    float3 interpolatedNormal = n0 * (1.0f - barycentrics.x - barycentrics.y) +
                               n1 * barycentrics.x +
                               n2 * barycentrics.y;
    
    return normalize(interpolatedNormal);
}

// ★★★ ワールド変換版の法線取得 ★★★
float3 GetWorldNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    // ローカル法線を取得
    float3 localNormal = GetInterpolatedNormal(instanceID, primitiveID, barycentrics);
    
    // ワールド変換を適用
    float3x4 objectToWorld = ObjectToWorld3x4();
    
    // 法線変換（回転のみ）
    float3 worldNormal = float3(
        dot(localNormal, objectToWorld._m00_m10_m20),
        dot(localNormal, objectToWorld._m01_m11_m21),
        dot(localNormal, objectToWorld._m02_m12_m22)
    );
    
    return normalize(worldNormal);
}
#endif // COMMON_HLSLI