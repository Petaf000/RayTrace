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
    InstanceOffsetData instanceData = InstanceOffsetBuffer[instanceID];
    
    // TODO: CPP側でマテリアルがない時の処理を追加する
    /*if (instanceData.materialID == 0xFFFFFFFF)
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
    }*/
    return MaterialBuffer[instanceData.materialID];
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
    // オフセット情報を取得
    InstanceOffsetData offset = InstanceOffsetBuffer[instanceID];
    
    // 三角形のインデックスを取得  
    uint baseIndex = offset.indexOffset + primitiveID * 3;
    
    // 頂点インデックス取得
    uint i0 = IndexBuffer[baseIndex + 0] + offset.vertexOffset;
    uint i1 = IndexBuffer[baseIndex + 1] + offset.vertexOffset;
    uint i2 = IndexBuffer[baseIndex + 2] + offset.vertexOffset;
    
    // 頂点法線を重心座標で補間
    float3 normal = VertexBuffer[i0].normal * (1.0f - barycentrics.x - barycentrics.y) +
                    VertexBuffer[i1].normal * barycentrics.x +
                    VertexBuffer[i2].normal * barycentrics.y;
    
    return normalize(normal);
}

// ★★★ ワールド変換版の法線取得（修正版） ★★★
float3 GetWorldNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    // ローカル法線を取得
    float3 localNormal = GetInterpolatedNormal(instanceID, primitiveID, barycentrics);

    // ワールド変換行列を取得
    float3x4 objectToWorld = ObjectToWorld3x4();

    // 法線をワールド空間に変換
    // objectToWorldの左上の3x3部分（回転とスケーリングを担う）を乗算する
    float3 worldNormal = mul((float3x3) objectToWorld, localNormal);

    // 変換後の法線を正規化して返す
    return normalize(worldNormal);
}
#endif // COMMON_HLSLI