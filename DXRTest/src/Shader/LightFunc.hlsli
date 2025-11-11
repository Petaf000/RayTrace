#ifndef LIGHTFUNC_HLSLI
#define LIGHTFUNC_HLSLI
#include "Utils.hlsli"

float LightSamplePDF(float3 worldPos, float3 direction, float distance)
{
    LightInfo light = GetLightInfo();
    
    float3 lightCenter = light.position;
    float3 toLight = normalize(lightCenter - worldPos);
    
    if (dot(normalize(direction), toLight) > 0.99f)    {
        float distanceSquared = distance * distance;
        float cosTheta = max(0.0f, dot(-normalize(direction), light.normal));
        if (cosTheta > 0.0f)
        {
            return distanceSquared / (light.area * cosTheta);
        }
    }
    
    return 0.0f;
}

float CosinePDF(float3 direction, float3 normal)
{
    float cosTheta = max(0.0f, dot(direction, normal));
    return cosTheta / PI;
}

float PowerHeuristic(int nf, float fPdf, int ng, float gPdf)
{
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f * f) / (f * f + g * g);
}

float BalanceHeuristic(float lightPdf, float brdfPdf)
{
    return lightPdf / (lightPdf + brdfPdf);
}

float PowerHeuristic2(float lightPdf, float brdfPdf)
{
    float lightSq = lightPdf * lightPdf;
    float brdfSq = brdfPdf * brdfPdf;
    return lightSq / (lightSq + brdfSq);
}

float MISWeightLight(float lightPdf, float brdfPdf)
{
    if (lightPdf <= 0.0f) return 0.0f;
    if (brdfPdf <= 0.0f) return 1.0f;
    
    return PowerHeuristic2(lightPdf, brdfPdf);
}

float MISWeightBRDF(float lightPdf, float brdfPdf)
{
    if (brdfPdf <= 0.0f) return 0.0f;
    if (lightPdf <= 0.0f) return 1.0f;
    
    return PowerHeuristic2(brdfPdf, lightPdf);
}

float CalculateLightPDF(uint lightIndex, float3 worldPos, float3 lightSamplePos, float3 lightDirection)
{
    if (lightIndex >= numLights) return 0.0f;
    
    LightInfo light = LightBuffer[lightIndex];
    
    if (light.lightType == 0) {        float distance = length(lightSamplePos - worldPos);
        float cosTheta = max(0.0f, dot(-lightDirection, light.normal));
        
        if (cosTheta > 0.001f && distance > 0.001f) {
            float distanceSquared = distance * distance;
            return distanceSquared / (cosTheta * light.area);
        }
    }
    else if (light.lightType == 1) {        float distance = length(light.position - worldPos);
        if (distance > 0.001f) {
            float distanceSquared = distance * distance;
            return 1.0f / (4.0f * PI * distanceSquared);
        }
    }
    
    return 0.0f;
}

LightSample SampleAreaLight(float3 worldPos, inout uint seed)
{
    LightSample sample;
    LightInfo light = GetLightInfo();
    
    float u = RandomFloat(seed);
    float v = RandomFloat(seed);
    
    float3 lightSamplePos = light.position + float3(
        (u - 0.5f) * light.size.x,
        0.0f,
        (v - 0.5f) * light.size.z
    );
    
    float3 toLight = lightSamplePos - worldPos;
    sample.distance = length(toLight);
    sample.direction = normalize(toLight);
    sample.position = lightSamplePos;
    
    float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
    
    if (cosTheta > 0.001f && sample.distance > 0.001f)
    {
        float distanceSquared = sample.distance * sample.distance;
        sample.pdf = distanceSquared / (cosTheta * light.area);
        
        sample.pdf = max(sample.pdf, 0.0001f);        
        sample.radiance = light.emission;
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
    }
    
    return sample;
}

LightSample SampleLightByIndexStratified(uint lightIndex, float3 worldPos, inout uint seed, uint stratumX, uint stratumY, uint strataCountX, uint strataCountY)
{
    LightSample sample;
    sample.valid = false;
    
    if (lightIndex >= numLights) {
        return sample;
    }
    
    LightInfo light = LightBuffer[lightIndex];
    
    if (light.lightType == 0) {        float u = RandomFloat(seed);
        float v = RandomFloat(seed);
        
        float cellU = (float(stratumX) + u) / float(strataCountX);
        float cellV = (float(stratumY) + v) / float(strataCountY);
        
        float safeMargin = 0.05f;        cellU = safeMargin + cellU * (1.0f - 2.0f * safeMargin);
        cellV = safeMargin + cellV * (1.0f - 2.0f * safeMargin);
        
        float3 lightSamplePos = light.position + float3(
            (cellU - 0.5f) * light.size.x,
            (v - 0.5f) * light.size.y,            (cellV - 0.5f) * light.size.z
        );
        
        float3 toLight = lightSamplePos - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = lightSamplePos;
        
        float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
        
        if (sample.distance > 0.001f && cosTheta > 0.01f) {            float distanceSquared = sample.distance * sample.distance;
            float rawPdf = distanceSquared / (cosTheta * light.area);
            
            sample.pdf = min(rawPdf, 1000.0f);            sample.radiance = light.emission;
            sample.valid = true;
        }
    }
    else if (light.lightType == 1) {        float3 toLight = light.position - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = light.position;
        
        if (sample.distance > 0.001f) {
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = 1.0f / (4.0f * PI * distanceSquared);
            sample.radiance = light.emission / distanceSquared;
            sample.valid = true;
        }
    }
    
    return sample;
}

LightSample SampleLightByIndex(uint lightIndex, float3 worldPos, inout uint seed)
{
    LightSample sample;
    sample.valid = false;
    if (lightIndex >= numLights) {
        return sample;
    }
    
    LightInfo light = LightBuffer[lightIndex];
    
    float u = RandomFloat(seed);
    float v = RandomFloat(seed);
    
    if (light.lightType == 0) {        float3 lightSamplePos = light.position + float3(
            (u - 0.5f) * light.size.x,
            0.0f,
            (v - 0.5f) * light.size.z
        );
        
        float3 toLight = lightSamplePos - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = lightSamplePos;
        
        float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
        
        if (sample.distance > 0.001f && cosTheta > 0.001f) {
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = distanceSquared / (cosTheta * light.area);
            
            sample.radiance = light.emission;
            sample.valid = true;
        }
    }
    else if (light.lightType == 1) {        float3 toLight = light.position - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = light.position;
        
        if (sample.distance > 0.001f) {
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = 1.0f / (4.0f * PI * distanceSquared);
            sample.radiance = light.emission / distanceSquared;            sample.valid = true;
        }
    }
    
    return sample;
}

static const float2 PoissonDisk8[8] = {
    float2(-0.7071f, 0.7071f),    float2(-0.0000f, 1.0000f),    float2(0.7071f, 0.7071f),    float2(1.0000f, 0.0000f),    float2(0.7071f, -0.7071f),    float2(0.0000f, -1.0000f),    float2(-0.7071f, -0.7071f),    float2(-1.0000f, 0.0000f)};

LightSample SampleLightByIndexPoisson(uint lightIndex, float3 worldPos, inout uint seed, uint sampleIdx, uint totalSamples)
{
    LightSample sample;
    sample.valid = false;
    
    if (lightIndex >= numLights || sampleIdx >= totalSamples) {
        return sample;
    }
    
    LightInfo light = LightBuffer[lightIndex];
    
    if (light.lightType == 0) {        float2 diskOffset;
        
        if (totalSamples <= 8) {
            diskOffset = PoissonDisk8[sampleIdx % 8];
        } else {
            float angle = (float(sampleIdx) / float(totalSamples)) * 2.0f * PI + RandomFloat(seed) * 0.5f;
            float radius = sqrt(RandomFloat(seed)) * 0.8f;            diskOffset = float2(cos(angle), sin(angle)) * radius;
        }
        
        float rotation = RandomFloat(seed) * 2.0f * PI;
        float cosRot = cos(rotation);
        float sinRot = sin(rotation);
        float2 rotatedOffset = float2(
            diskOffset.x * cosRot - diskOffset.y * sinRot,
            diskOffset.x * sinRot + diskOffset.y * cosRot
        );
        
        float3 lightSamplePos = light.position + float3(
            rotatedOffset.x * light.size.x * 0.4f,
            0.0f,
            rotatedOffset.y * light.size.z * 0.4f
        );
        
        float3 toLight = lightSamplePos - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = lightSamplePos;
        
        float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
        
        if (sample.distance > 0.001f && cosTheta > 0.001f) {
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = distanceSquared / (cosTheta * light.area);
            sample.radiance = light.emission;
            sample.valid = true;
        }
    }
    else if (light.lightType == 1) {        float3 toLight = light.position - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = light.position;
        
        if (sample.distance > 0.001f) {
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = 1.0f / (4.0f * PI * distanceSquared);
            sample.radiance = light.emission / distanceSquared;
            sample.valid = true;
        }
    }
    
    return sample;
}

LightSample SampleRandomLight(float3 worldPos, inout uint seed)
{
    if (numLights == 0) {
        LightSample invalidSample;
        invalidSample.valid = false;
        return invalidSample;
    }
    
    uint lightIndex = uint(RandomFloat(seed) * float(numLights)) % numLights;
    return SampleLightByIndex(lightIndex, worldPos, seed);
}


#endif