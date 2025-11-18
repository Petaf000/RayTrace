#ifndef LIGHTFUNC_HLSLI
#define LIGHTFUNC_HLSLI

#include "Utils.hlsli"
#include "../Core/Types.hlsli"

// ===== 定数 =====
static const float LIGHT_SAMPLE_EPSILON = 0.001f;
static const float LIGHT_COS_THETA_MIN = 0.001f;
static const float MAX_LIGHT_PDF = 1000.0f;

// ===== MIS (数学) =====

// Power Heuristic (2乗バージョン)
float PowerHeuristicSq(float pdf1, float pdf2)
{
    float sq1 = pdf1 * pdf1;
    float sq2 = pdf2 * pdf2;
    return sq1 / (sq1 + sq2 + 1e-6f);
}

float MISWeightLight(float lightPdf, float brdfPdf)
{
    if (lightPdf <= 0.0f)
        return 0.0f;
    if (brdfPdf <= 0.0f)
        return 1.0f;
    return PowerHeuristicSq(lightPdf, brdfPdf);
}

float MISWeightBRDF(float lightPdf, float brdfPdf)
{
    if (brdfPdf <= 0.0f)
        return 0.0f;
    if (lightPdf <= 0.0f)
        return 1.0f;
    return PowerHeuristicSq(brdfPdf, lightPdf);
}

// ===== ライトサンプリング (データ取得) =====

void SampleAreaLight(LightInfo light, float3 lightSamplePos, float3 worldPos, out LightSample sample)
{
    sample.position = lightSamplePos;
    float3 toLight = lightSamplePos - worldPos;
    sample.distance = length(toLight);
    sample.direction = normalize(toLight);
    
    float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
    
    if (sample.distance > LIGHT_SAMPLE_EPSILON && cosTheta > LIGHT_COS_THETA_MIN)
    {
        float distanceSquared = sample.distance * sample.distance;
        sample.pdf = distanceSquared / (cosTheta * light.area);
        sample.pdf = max(sample.pdf, 1e-6f);
        sample.radiance = light.emission;
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
        sample.pdf = 0.0f;
        sample.radiance = float3(0, 0, 0);
    }
}

void SamplePointLight(LightInfo light, float3 worldPos, out LightSample sample)
{
    sample.position = light.position;
    float3 toLight = light.position - worldPos;
    sample.distance = length(toLight);
    sample.direction = normalize(toLight);
    
    if (sample.distance > LIGHT_SAMPLE_EPSILON)
    {
        float distanceSquared = sample.distance * sample.distance;
        sample.pdf = 1.0f / (4.0f * PI * distanceSquared);
        sample.radiance = light.emission / distanceSquared;
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
        sample.pdf = 0.0f;
        sample.radiance = float3(0, 0, 0);
    }
}

LightSample SampleLightByIndex(uint lightIndex, float3 worldPos, inout uint seed)
{
    LightSample sample;
    sample.valid = false;
    
    if (lightIndex >= numLights)
        return sample;
    LightInfo light = LightBuffer[lightIndex];
    
    if (light.lightType == 0)
    {
        float u = RandomFloat(seed);
        float v = RandomFloat(seed);
        float3 lightSamplePos = light.position + float3((u - 0.5f) * light.size.x, 0.0f, (v - 0.5f) * light.size.z);
        SampleAreaLight(light, lightSamplePos, worldPos, sample);
    }
    else if (light.lightType == 1)
    {
        SamplePointLight(light, worldPos, sample);
    }
    return sample;
}

// ===== シャドウレイ (可視性判定) =====

bool TestLightVisibility(float3 worldPos, float3 normal, float3 lightDirection, float lightDistance, uint seed)
{
    RayDesc shadowRay;
    shadowRay.Origin = OffsetRay(worldPos, normal);
    shadowRay.Direction = lightDirection;
    shadowRay.TMin = LIGHT_SAMPLE_EPSILON;
    shadowRay.TMax = max(0.01f, lightDistance - LIGHT_SAMPLE_EPSILON);
    
    RayPayload shadowPayload;
    shadowPayload.color = float3(1, 1, 1);
    shadowPayload.depth = 999;
    shadowPayload.seed = seed;
    
    // 警告回避の初期化
    shadowPayload.albedo = float3(0, 0, 0);
    shadowPayload.normal = float3(0, 0, 1);
    shadowPayload.worldPos = float3(0, 0, 0);
    shadowPayload.hitDistance = 0;
    shadowPayload.materialType = 0;
    shadowPayload.roughness = 0;
    shadowPayload.padding = 0;

    TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
             0xFF, 0, 1, 0, shadowRay, shadowPayload);
             
    return length(shadowPayload.color) > 0.5f;
}

#endif