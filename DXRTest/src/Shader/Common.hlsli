#ifndef COMMON_HLSLI
#define COMMON_HLSLI

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

#include "Utils.hlsli"
#include "GeometryData.hlsli"
#include "LightData.hlsli"

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

MaterialData GetMaterial(uint instanceID)
{
    InstanceOffsetData instanceData = InstanceOffsetBuffer[instanceID];
    
    /*if (instanceData.materialID == 0xFFFFFFFF)
    {
        MaterialData defaultMaterial;
        defaultMaterial.albedo = float3(0.0f, 0.0f, 1.0f);
        defaultMaterial.roughness = 1.0f;
        defaultMaterial.refractiveIndex = 1.0f;
        defaultMaterial.emission = float3(0, 0, 0);
        defaultMaterial.materialType = MATERIAL_LAMBERTIAN;
        defaultMaterial.padding = 0.0f;
        return defaultMaterial;
    }*/
    return MaterialBuffer[instanceData.materialID];
}

float3 NormalToColor(float3 normal)
{
    return normal * 0.5f + 0.5f;
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

LightInfo GetLightInfo()
{
    if (numLights > 0)
    {
        return LightBuffer[0];
    }
    
    LightInfo light;
    light.position = float3(0.0f, 267.5f, -227.0f);
    light.emission = float3(15.0f, 15.0f, 15.0f);
    light.size = float3(130.0f, 5.0f, 105.0f);
    light.area = light.size.x * light.size.z;
    light.normal = float3(0, -1, 0);
    light.lightType = 0;
    light.instanceID = 5;
    return light;
}

#include "LightFunc.hlsli"

#endif