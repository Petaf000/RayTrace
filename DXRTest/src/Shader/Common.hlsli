#ifndef COMMON_HLSLI
#define COMMON_HLSLI

// 構造体等定義ファイル
#include "Core/Types.hlsli"

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    
    float3 cameraRight;
    float tanHalfFov;
    
    float3 cameraUp;
    float aspectRatio;
    
    float3 cameraForward;
    float frameCount;
    
    uint numLights;
    uint cameraMovedFlag;
    float2 padding;
};

// リソース定義
RaytracingAccelerationStructure SceneBVH : register(t0);
RWTexture2D<float4> RenderTarget : register(u0);

RWTexture2D<float4> AccumulationBuffer : register(u1);
RWTexture2D<float4> PrevFrameData : register(u2);
RWTexture2D<float4> AlbedoOutput : register(u3);
RWTexture2D<float4> NormalOutput : register(u4);
RWTexture2D<float4> DepthOutput : register(u5);

RWStructuredBuffer<LightReservoir> CurrentReservoirs : register(u6);
RWStructuredBuffer<LightReservoir> PreviousReservoirs : register(u7);

StructuredBuffer<MaterialData> MaterialBuffer : register(t1);
StructuredBuffer<DXRVertex> VertexBuffer : register(t2);
StructuredBuffer<uint> IndexBuffer : register(t3);
StructuredBuffer<InstanceOffsetData> InstanceOffsetBuffer : register(t4);
StructuredBuffer<LightInfo> LightBuffer : register(t5);

#include "Lib/Utils.hlsli"

MaterialData GetMaterial(uint instanceID)
{
    InstanceOffsetData instanceData = InstanceOffsetBuffer[instanceID];
    return MaterialBuffer[instanceData.materialID];
}

float3 GetInterpolatedNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    InstanceOffsetData offset = InstanceOffsetBuffer[instanceID];
    uint baseIndex = offset.indexOffset + primitiveID * 3;
    
    uint i0 = IndexBuffer[baseIndex + 0] + offset.vertexOffset;
    uint i1 = IndexBuffer[baseIndex + 1] + offset.vertexOffset;
    uint i2 = IndexBuffer[baseIndex + 2] + offset.vertexOffset;
    
    float3 normal = VertexBuffer[i0].normal * (1.0f - barycentrics.x - barycentrics.y) +
                    VertexBuffer[i1].normal * barycentrics.x +
                    VertexBuffer[i2].normal * barycentrics.y;
    return normalize(normal);
}

float3 GetWorldNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    float3 localNormal = GetInterpolatedNormal(instanceID, primitiveID, barycentrics);
    float3x4 objectToWorld = ObjectToWorld3x4();

    float3 worldNormal = mul((float3x3) objectToWorld, localNormal);

    return normalize(worldNormal);
}

// インクルード順のため、ここでLightFuncをインクルード
#include "Lib/LightFunc.hlsli"

#endif