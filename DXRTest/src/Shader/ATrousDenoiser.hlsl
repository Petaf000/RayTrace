#include "Common.hlsli"

// A-Trous Wavelet Denoiser Compute Shader
// Using global root signature

// G-Buffer: t6(Albedo), t7(Normal), t8(Depth) - 現在のバインド構成に合わせて修正
Texture2D<float4> AlbedoBuffer : register(t6);
Texture2D<float4> NormalBuffer : register(t7);
Texture2D<float4> DepthBuffer : register(t8);
RWTexture2D<float4> DenoiserOutput : register(u6);

// デノイザー用定数バッファ
cbuffer DenoiserConstants : register(b1)
{
    int stepSize;
    float colorSigma;      // 推奨値: 0.2-0.4 (間接光ノイズ用)
    float normalSigma;     // 推奨値: 0.5-1.0 (法線感度)
    float depthSigma;      // 推奨値: 0.1-0.3 (深度感度)
    float2 texelSize;
    float2 denoiserPadding;  // Common.hlsliのpaddingとの名前競合を回避
}

// 5x5 A-Trous カーネル (B3-spline interpolation)
static const float kernel[25] =
{
    1.0f / 256.0f, 4.0f / 256.0f, 6.0f / 256.0f, 4.0f / 256.0f, 1.0f / 256.0f,
    4.0f / 256.0f, 16.0f / 256.0f, 24.0f / 256.0f, 16.0f / 256.0f, 4.0f / 256.0f,
    6.0f / 256.0f, 24.0f / 256.0f, 36.0f / 256.0f, 24.0f / 256.0f, 6.0f / 256.0f,
    4.0f / 256.0f, 16.0f / 256.0f, 24.0f / 256.0f, 16.0f / 256.0f, 4.0f / 256.0f,
    1.0f / 256.0f, 4.0f / 256.0f, 6.0f / 256.0f, 4.0f / 256.0f, 1.0f / 256.0f
};

// 5x5カーネルオフセット
static const int2 offsets[25] =
{
    int2(-2, -2), int2(-1, -2), int2(0, -2), int2(1, -2), int2(2, -2),
    int2(-2, -1), int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1),
    int2(-2, 0), int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0),
    int2(-2, 1), int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1),
    int2(-2, 2), int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2)
};

// === 重み計算の関数 ===
float ComputeWeight(float3 centerColor, float3 sampleColor,
                   float3 centerNormal, float3 sampleNormal,
                   float centerDepth, float sampleDepth)
{
    // カラー重み（より平滑に）
    float3 colorDiff = centerColor - sampleColor;
    float colorDistance = length(colorDiff);
    float colorWeight = exp(-colorDistance * colorDistance / (2.0f * colorSigma * colorSigma));
    
    // 法線重み（修正版：cosine similarity使用）
    float normalDot = max(0.0f, dot(centerNormal, sampleNormal));
    float normalWeight = pow(normalDot, normalSigma);
    
    // 深度重み（修正版：より厳密に）
    float depthDiff = abs(centerDepth - sampleDepth);
    float depthWeight = exp(-depthDiff * depthDiff / (2.0f * depthSigma * depthSigma));
    
    // 組み合わせ重み
    return colorWeight * normalWeight * depthWeight;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    RenderTarget.GetDimensions(dims.x, dims.y);
    
    // 範囲チェック
    if (id.x >= dims.x || id.y >= dims.y)
        return;
    
    int2 centerCoord = int2(id.xy);
    
    // 中心ピクセルの値を取得（RenderTargetから直接読み取り）
    float4 centerColor = RenderTarget[centerCoord];
    float3 centerAlbedo = AlbedoBuffer[centerCoord].rgb;
    float3 centerNormal = normalize(NormalBuffer[centerCoord].xyz);
    float centerDepth = DepthBuffer[centerCoord].r;
    
    // NaN/無限大チェック（詳細デバッグ）
    if (any(isnan(centerColor.rgb)) || any(isinf(centerColor.rgb)))
    {
        DenoiserOutput[id.xy] = float4(1, 0, 0, 1); // 赤：RenderTarget（centerColor）にNaN/Inf
        return;
    }
    if (any(isnan(centerNormal)) || any(isinf(centerNormal)))
    {
        // Normal値の詳細デバッグ
        float3 rawNormal = NormalBuffer[centerCoord].xyz;
        
        if (isnan(rawNormal.x)) {
            DenoiserOutput[id.xy] = float4(1, 0.5f, 0.5f, 1); // 赤系：X成分がNaN
        } else if (isnan(rawNormal.y)) {
            DenoiserOutput[id.xy] = float4(0.5f, 1, 0.5f, 1); // 緑系：Y成分がNaN  
        } else if (isnan(rawNormal.z)) {
            DenoiserOutput[id.xy] = float4(0.5f, 0.5f, 1, 1); // 青系：Z成分がNaN
        } else if (any(isinf(rawNormal))) {
            DenoiserOutput[id.xy] = float4(1, 1, 1, 1); // 白：Inf値
        } else {
            DenoiserOutput[id.xy] = float4(0, 1, 0, 1); // 緑：normalize()でNaN
        }
        return;
    }
    if (isnan(centerDepth) || isinf(centerDepth))
    {
        DenoiserOutput[id.xy] = float4(0, 0, 1, 1); // 青：DepthBufferにNaN/Inf
        return;
    }
    
    // AlbedoBufferのチェック（既に取得済み）
    if (any(isnan(centerAlbedo)) || any(isinf(centerAlbedo)))
    {
        DenoiserOutput[id.xy] = float4(1, 1, 0, 1); // 黄：AlbedoBufferにNaN/Inf
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
        float4 sampleColor = RenderTarget[sampleCoord];
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
        
        // カーネル重みとエッジ保護重みを計算
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
    
    // アルファ値を保護
    filteredColor.a = centerColor.a;
    
    DenoiserOutput[id.xy] = filteredColor;
}