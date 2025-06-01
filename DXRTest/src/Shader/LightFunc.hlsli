#ifndef LIGHTFUNC_HLSLI
#define LIGHTFUNC_HLSLI
#include "Utils.hlsli"
#include "LightData.hlsli"

// PDF値の計算（CPUレイトレと同じ実装）
float LightSamplePDF(float3 worldPos, float3 direction, float distance)
{
    LightInfo light = GetLightInfo();
    
    // レイと光源の交差をテスト
    // 簡略化：方向から光源への到達を確認
    float3 lightCenter = light.position;
    float3 toLight = normalize(lightCenter - worldPos);
    
    // 方向の類似性をチェック
    if (dot(normalize(direction), toLight) > 0.99f) // 大体同じ方向
    {
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

// MIS重み計算（Power Heuristic）
float PowerHeuristic(int nf, float fPdf, int ng, float gPdf)
{
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f * f) / (f * f + g * g);
}

// ★★★ CPUコード準拠のエリアライトサンプリング ★★★
LightSample SampleAreaLight(float3 worldPos, inout uint seed)
{
    LightSample sample;
    LightInfo light = GetLightInfo();
    
    float u = RandomFloat(seed);
    float v = RandomFloat(seed);
    
    // XZ平面での矩形サンプリング
    float3 lightSamplePos = light.position + float3(
        (u - 0.5f) * light.size.x,
        0.0f,
        (v - 0.5f) * light.size.z
    );
    
    // 方向と距離
    float3 toLight = lightSamplePos - worldPos;
    sample.distance = length(toLight);
    sample.direction = normalize(toLight);
    sample.position = lightSamplePos;
    
    // ライトの法線との内積（下向きライト）
    float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
    
    if (cosTheta > 0.001f && sample.distance > 0.001f)
    {
        // ★★★ 修正: 正しい立体角PDF（距離減衰込み） ★★★
        float distanceSquared = sample.distance * sample.distance;
        sample.pdf = distanceSquared / (cosTheta * light.area);
        
        // ★★★ 安全な範囲にクランプ ★★★
        sample.pdf = max(sample.pdf, 0.0001f); // 最小PDF値
        
        sample.radiance = light.emission;
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
    }
    
    return sample;
}


#endif