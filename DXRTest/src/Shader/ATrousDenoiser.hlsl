#include "Common.hlsli"

Texture2D<float4> AlbedoBuffer : register(t6);
Texture2D<float4> NormalBuffer : register(t7);
Texture2D<float4> DepthBuffer : register(t8);
RWTexture2D<float4> DenoiserOutput : register(u6);

cbuffer DenoiserConstants : register(b1)
{
    int stepSize;
    float colorSigma;
    float normalSigma;
    float depthSigma;
    float2 texelSize;
    float2 denoiserPadding;
}

static const float kernel[25] =
{
    1.0f / 256.0f, 4.0f / 256.0f, 6.0f / 256.0f, 4.0f / 256.0f, 1.0f / 256.0f,
    4.0f / 256.0f, 16.0f / 256.0f, 24.0f / 256.0f, 16.0f / 256.0f, 4.0f / 256.0f,
    6.0f / 256.0f, 24.0f / 256.0f, 36.0f / 256.0f, 24.0f / 256.0f, 6.0f / 256.0f,
    4.0f / 256.0f, 16.0f / 256.0f, 24.0f / 256.0f, 16.0f / 256.0f, 4.0f / 256.0f,
    1.0f / 256.0f, 4.0f / 256.0f, 6.0f / 256.0f, 4.0f / 256.0f, 1.0f / 256.0f
};

static const int2 offsets[25] =
{
    int2(-2, -2), int2(-1, -2), int2(0, -2), int2(1, -2), int2(2, -2),
    int2(-2, -1), int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1),
    int2(-2, 0), int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0),
    int2(-2, 1), int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1),
    int2(-2, 2), int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2)
};

float ComputeWeight(float3 centerColor, float3 sampleColor,
                   float3 centerNormal, float3 sampleNormal,
                   float centerDepth, float sampleDepth)
{
    float3 colorDiff = centerColor - sampleColor;
    float colorDistance = length(colorDiff);
    float colorWeight = exp(-colorDistance * colorDistance / (2.0f * colorSigma * colorSigma));
    
    float normalDot = max(0.0f, dot(centerNormal, sampleNormal));
    float normalWeight = pow(normalDot, normalSigma);
    
    float depthDiff = abs(centerDepth - sampleDepth);
    float depthWeight = exp(-depthDiff * depthDiff / (2.0f * depthSigma * depthSigma));
    
    return colorWeight * normalWeight * depthWeight;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    RenderTarget.GetDimensions(dims.x, dims.y);
    
    if (id.x >= dims.x || id.y >= dims.y)
        return;
    
    int2 centerCoord = int2(id.xy);
    
    float4 centerColor = RenderTarget[centerCoord];
    float3 centerAlbedo = AlbedoBuffer[centerCoord].rgb;
    float3 centerNormal = normalize(NormalBuffer[centerCoord].xyz);
    float centerDepth = DepthBuffer[centerCoord].r;
    
    if (any(isnan(centerColor.rgb)) || any(isinf(centerColor.rgb)))
    {
        DenoiserOutput[id.xy] = float4(1, 0, 0, 1);        return;
    }
    if (any(isnan(centerNormal)) || any(isinf(centerNormal)))
    {

        float3 rawNormal = NormalBuffer[centerCoord].xyz;
        
        if (isnan(rawNormal.x)) {
            DenoiserOutput[id.xy] = float4(1, 0.5f, 0.5f, 1);
        } else if (isnan(rawNormal.y)) {
            DenoiserOutput[id.xy] = float4(0.5f, 1, 0.5f, 1);
        } else if (isnan(rawNormal.z)) {
            DenoiserOutput[id.xy] = float4(0.5f, 0.5f, 1, 1);
        } else if (any(isinf(rawNormal))) {
            DenoiserOutput[id.xy] = float4(1, 1, 1, 1);
        } else {
            DenoiserOutput[id.xy] = float4(0, 1, 0, 1);
        }
        return;
    }
    if (isnan(centerDepth) || isinf(centerDepth))
    {
        DenoiserOutput[id.xy] = float4(0, 0, 1, 1);
        return;
    }
    

    if (any(isnan(centerAlbedo)) || any(isinf(centerAlbedo)))
    {
        DenoiserOutput[id.xy] = float4(1, 1, 0, 1);
        return;
    }
    

    if (length(centerNormal) < 0.1f || centerDepth <= 0.0f)
    {

        DenoiserOutput[id.xy] = centerColor;
        return;
    }
    

    float4 filteredColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0f;
    

    for (int i = 0; i < 25; ++i)
    {

        int2 sampleCoord = centerCoord + offsets[i] * stepSize;
        

        if (sampleCoord.x < 0 || sampleCoord.x >= (int) dims.x ||
            sampleCoord.y < 0 || sampleCoord.y >= (int) dims.y)
        {
            continue;
        }
        

        float4 sampleColor = RenderTarget[sampleCoord];
        float3 sampleAlbedo = AlbedoBuffer[sampleCoord].rgb;
        float3 sampleNormal = normalize(NormalBuffer[sampleCoord].xyz);
        float sampleDepth = DepthBuffer[sampleCoord].r;
        

        if (any(isnan(sampleColor.rgb)) || any(isinf(sampleColor.rgb)) ||
            any(isnan(sampleNormal)) || any(isinf(sampleNormal)) ||
            isnan(sampleDepth) || isinf(sampleDepth))
            continue;
            
        if (length(sampleNormal) < 0.1f || sampleDepth <= 0.0f)
            continue;
        
        float kernelWeight = kernel[i];
        float edgeWeight = ComputeWeight(centerColor.rgb, sampleColor.rgb,
                                       centerNormal, sampleNormal,
                                       centerDepth, sampleDepth);
        
        float weight = kernelWeight * edgeWeight;
        
        filteredColor += sampleColor * weight;
        totalWeight += weight;
    }
    
    if (totalWeight > 0.001f)
    {
        filteredColor /= totalWeight;
    }
    else
    {
        filteredColor = centerColor;
    }
    
    if (any(isnan(filteredColor.rgb)) || any(isinf(filteredColor.rgb)))
    {
        filteredColor = centerColor;
    }
    
    filteredColor.a = centerColor.a;
    
    DenoiserOutput[id.xy] = filteredColor;
}