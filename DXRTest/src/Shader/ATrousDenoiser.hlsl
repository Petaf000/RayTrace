#include "Common.hlsli"

// A-Trous Wavelet Denoiser Compute Shader
// Using global root signature - レジスタは既存のG-Buffer用を使用

// G-Buffer: t5(Albedo), t6(Normal), t7(Depth) を使用
Texture2D<float4> AlbedoBuffer : register(t5); // G-Bufferのアルベド  
Texture2D<float4> NormalBuffer : register(t6); // G-Bufferの法線
Texture2D<float4> DepthBuffer : register(t7); // G-Bufferの深度
RWTexture2D<float4> DenoiserOutput : register(u4); // デノイザー出力

// デノイザー用定数バッファ
cbuffer DenoiserConstants : register(b1)
{
    int stepSize; // A-Trousステップサイズ (2^iteration)
    float colorSigma; // カラー重み調整
    float normalSigma; // 法線重み調整 
    float depthSigma; // 深度重み調整
    float2 texelSize; // テクセルサイズ (1.0/resolution)
    float2 padding;
}

// 5x5 A-Trous カーネル (B3-spline interpolation)
static const float kernel[49] =
{
    1.0f / 1024.0f, 3.0f / 1024.0f, 7.0f / 1024.0f, 12.0f / 1024.0f, 7.0f / 1024.0f, 3.0f / 1024.0f, 1.0f / 1024.0f,
    3.0f / 1024.0f, 9.0f / 1024.0f, 21.0f / 1024.0f, 36.0f / 1024.0f, 21.0f / 1024.0f, 9.0f / 1024.0f, 3.0f / 1024.0f,
    7.0f / 1024.0f, 21.0f / 1024.0f, 49.0f / 1024.0f, 84.0f / 1024.0f, 49.0f / 1024.0f, 21.0f / 1024.0f, 7.0f / 1024.0f,
    12.0f / 1024.0f, 36.0f / 1024.0f, 84.0f / 1024.0f, 144.0f / 1024.0f, 84.0f / 1024.0f, 36.0f / 1024.0f, 12.0f / 1024.0f,
    7.0f / 1024.0f, 21.0f / 1024.0f, 49.0f / 1024.0f, 84.0f / 1024.0f, 49.0f / 1024.0f, 21.0f / 1024.0f, 7.0f / 1024.0f,
    3.0f / 1024.0f, 9.0f / 1024.0f, 21.0f / 1024.0f, 36.0f / 1024.0f, 21.0f / 1024.0f, 9.0f / 1024.0f, 3.0f / 1024.0f,
    1.0f / 1024.0f, 3.0f / 1024.0f, 7.0f / 1024.0f, 12.0f / 1024.0f, 7.0f / 1024.0f, 3.0f / 1024.0f, 1.0f / 1024.0f
};

// 7x7カーネルオフセット
static const int2 offsets[49] =
{
    int2(-3, -3), int2(-2, -3), int2(-1, -3), int2(0, -3), int2(1, -3), int2(2, -3), int2(3, -3),
    int2(-3, -2), int2(-2, -2), int2(-1, -2), int2(0, -2), int2(1, -2), int2(2, -2), int2(3, -2),
    int2(-3, -1), int2(-2, -1), int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1), int2(3, -1),
    int2(-3, 0), int2(-2, 0), int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0), int2(3, 0),
    int2(-3, 1), int2(-2, 1), int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1), int2(3, 1),
    int2(-3, 2), int2(-2, 2), int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2), int2(3, 2),
    int2(-3, 3), int2(-2, 3), int2(-1, 3), int2(0, 3), int2(1, 3), int2(2, 3), int2(3, 3)
};

// === 重み計算の改善 ===
float ComputeWeight(float3 centerColor, float3 sampleColor,
                   float3 centerNormal, float3 sampleNormal,
                   float centerDepth, float sampleDepth)
{
    // カラー重み（より敏感に）
    float3 colorDiff = centerColor - sampleColor;
    float colorDistance = length(colorDiff);
    float colorWeight = exp(-colorDistance / max(colorSigma, 0.001f));
    
    // 法線重み（より厳しく）
    float normalDot = max(0.0f, dot(centerNormal, sampleNormal));
    float normalWeight = pow(normalDot, max(normalSigma, 0.001f));
    
    // 深度重み（改善版）
    float depthDiff = abs(centerDepth - sampleDepth);
    float depthWeight = exp(-depthDiff / max(depthSigma, 0.001f));
    
    // 組み合わせ重み（バランス調整）
    return colorWeight * normalWeight * depthWeight;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    RenderTarget.GetDimensions(dims.x, dims.y); // Common.hlsliのRenderTargetを使用
    
    // 範囲チェック
    if (id.x >= dims.x || id.y >= dims.y)
        return;
    
    int2 centerCoord = int2(id.xy);
    
    // 中心ピクセルの値を取得（RenderTargetから直接読み取り）
    float4 centerColor = RenderTarget[centerCoord];
    float3 centerAlbedo = AlbedoBuffer[centerCoord].rgb;
    float3 centerNormal = normalize(NormalBuffer[centerCoord].xyz);
    float centerDepth = DepthBuffer[centerCoord].r;
    
    // NaN/無限大チェック
    if (any(isnan(centerColor.rgb)) || any(isinf(centerColor.rgb)) ||
        any(isnan(centerNormal)) || any(isinf(centerNormal)) ||
        isnan(centerDepth) || isinf(centerDepth))
    {
        DenoiserOutput[id.xy] = float4(1, 0, 1, 1); // マゼンタでエラー表示
        return;
    }
    
    // デフォルト値のチェック (G-Bufferが生成されていない場合)
    if (length(centerNormal) < 0.1f || centerDepth <= 0.0f)
    {
        // G-Bufferデータがない場合は元の色をそのまま出力
        DenoiserOutput[id.xy] = centerColor;
        return;
    }
    
    // A-Trousフィルタリング
    float4 filteredColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0f;
    
    // 5x5カーネルを適用
    for (int i = 0; i < 25; ++i)
    {
        // A-Trousスタイル: ステップサイズを適用
        int2 sampleCoord = centerCoord + offsets[i] * stepSize;
        
        // 境界チェック
        if (sampleCoord.x < 0 || sampleCoord.x >= (int) dims.x ||
            sampleCoord.y < 0 || sampleCoord.y >= (int) dims.y)
        {
            continue;
        }
        
        // サンプル値を取得
        float4 sampleColor = RenderTarget[sampleCoord]; // Common.hlsliのRenderTargetを使用
        float3 sampleAlbedo = AlbedoBuffer[sampleCoord].rgb;
        float3 sampleNormal = normalize(NormalBuffer[sampleCoord].xyz);
        float sampleDepth = DepthBuffer[sampleCoord].r;
        
        // NaN/無限大チェック
        if (any(isnan(sampleColor.rgb)) || any(isinf(sampleColor.rgb)) ||
            any(isnan(sampleNormal)) || any(isinf(sampleNormal)) ||
            isnan(sampleDepth) || isinf(sampleDepth))
            continue;
            
        // 無効なG-Bufferデータのチェック
        if (length(sampleNormal) < 0.1f || sampleDepth <= 0.0f)
            continue;
        
        // カーネル重みとエッジ保持重みを計算
        float kernelWeight = kernel[i];
        float edgeWeight = ComputeWeight(centerColor.rgb, sampleColor.rgb,
                                       centerNormal, sampleNormal,
                                       centerDepth, sampleDepth);
        
        float weight = kernelWeight * edgeWeight;
        
        filteredColor += sampleColor * weight;
        totalWeight += weight;
    }
    
    // 正規化
    if (totalWeight > 0.001f)
    {
        filteredColor /= totalWeight;
    }
    else
    {
        // フィルタリングに失敗した場合は元の色を使用
        filteredColor = centerColor;
    }
    
    // 最終チェック
    if (any(isnan(filteredColor.rgb)) || any(isinf(filteredColor.rgb)))
    {
        filteredColor = centerColor;
    }
    
    // アルファ値を保持
    filteredColor.a = centerColor.a;
    
    DenoiserOutput[id.xy] = filteredColor;
}