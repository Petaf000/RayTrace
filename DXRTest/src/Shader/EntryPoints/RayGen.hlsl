#include "../Common.hlsli"

static const float BlueNoise8x8[64] =
{
    0.515625f, 0.140625f, 0.890625f, 0.328125f, 0.484375f, 0.171875f, 0.921875f, 0.359375f,
    0.015625f, 0.765625f, 0.265625f, 0.703125f, 0.046875f, 0.796875f, 0.296875f, 0.734375f,
    0.640625f, 0.078125f, 0.828125f, 0.203125f, 0.671875f, 0.109375f, 0.859375f, 0.234375f,
    0.390625f, 0.953125f, 0.453125f, 0.578125f, 0.421875f, 0.984375f, 0.484375f, 0.609375f,
    0.546875f, 0.125000f, 0.859375f, 0.281250f, 0.515625f, 0.156250f, 0.890625f, 0.312500f,
    0.078125f, 0.734375f, 0.234375f, 0.656250f, 0.109375f, 0.765625f, 0.265625f, 0.687500f,
    0.703125f, 0.031250f, 0.796875f, 0.156250f, 0.734375f, 0.062500f, 0.828125f, 0.187500f,
    0.343750f, 0.906250f, 0.406250f, 0.531250f, 0.375000f, 0.937500f, 0.437500f, 0.562500f
};

uint GenerateBlueNoiseSeed(uint2 pixelCoord, uint frameIndex, uint sampleIndex)
{
    uint2 noiseCoord = pixelCoord & 7;
    uint noiseIndex = noiseCoord.y * 8 + noiseCoord.x;
    float blueNoiseValue = BlueNoise8x8[noiseIndex];
    
    uint blueNoiseSeed = uint(blueNoiseValue * 4294967295.0f);
    uint seed = blueNoiseSeed;
    seed ^= PCGHash(pixelCoord.x * 73856093u);
    seed ^= PCGHash(pixelCoord.y * 19349663u);
    seed ^= PCGHash(frameIndex * 83492791u);
    seed ^= PCGHash(sampleIndex * 51726139u);
    return PCGHash(seed);
}

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    // 累積用変数
    float3 finalColor = float3(0, 0, 0);
    float3 finalAlbedo = float3(0, 0, 0);
    float3 finalNormal = float3(0, 0, 0);
    float finalDepth = 0.0f;
    uint finalMaterialType = 0;
    float finalRoughness = 0.0f;
    
    // spp
    const int SAMPLES = 1;
    for (int sampleIndex = 0; sampleIndex < SAMPLES; sampleIndex++)
    {
        // フレームベースの乱数シード生成
        uint frameIndex = uint(frameCount) % 64u;
        uint seed = GenerateBlueNoiseSeed(launchIndex.xy, frameIndex, sampleIndex);
        
        // アンチエイリアシング用のジッター計算
        float2 jitter = float2(RandomFloat(seed), RandomFloat(seed)) - 0.5f;
        float2 crd = float2(launchIndex.xy) + jitter;
        float2 dims = float2(launchDim.xy);
        
        // NDC座標計算（-1から1の範囲）
        float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
        d.y = -d.y;
        
        // カメラ行列からレイの方向を計算
        float3 cameraPos = viewMatrix._m03_m13_m23;
        float3 rayDir = normalize(cameraForward +
                        d.x * cameraRight * tanHalfFov * aspectRatio +
                        d.y * cameraUp * tanHalfFov);
        
        // レイ構造体の設定
        RayDesc ray;
        ray.Origin = cameraPos;
        ray.Direction = rayDir;
        ray.TMin = 0.1f;
        ray.TMax = 10000.0f;
        
        // ペイロード初期化
        RayPayload payload;
        payload.color = float3(0, 0, 0);
        payload.depth = 0;
        payload.seed = seed;
        payload.albedo = float3(0, 0, 0);
        payload.normal = float3(0, 0, 1);
        payload.worldPos = float3(0, 0, 0);
        payload.hitDistance = 0.0f;
        payload.materialType = 0;
        payload.roughness = 0.0f;
        payload.padding = 0;
        
        // レイトレーシング実行
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
        
        // サンプル結果を累積
        finalColor += payload.color;
        
        // G-bufferデータは最初のサンプルのみ使用
        if (sampleIndex == 0)
        {
            finalAlbedo += payload.albedo;
            finalNormal += payload.normal;
            finalDepth += payload.hitDistance;
            finalMaterialType = payload.materialType;
            finalRoughness += payload.roughness;
        }
    }
    
    // 平均化
    finalColor /= float(SAMPLES);
    finalAlbedo /= float(SAMPLES);
    finalNormal /= float(SAMPLES);
    finalDepth /= float(SAMPLES);
    finalRoughness /= float(SAMPLES);
    
    // 無効な値の場合はデフォルト法線を設定
    if (length(finalNormal) < 0.1f)
        finalNormal = float3(0, 0, 1);
     
    finalNormal = normalize(finalNormal);
    
    float3 color = finalColor;
    
    // NaN/Inf チェック
    // 色が不正な場合はマゼンタで表示
    if (any(isnan(color)) || any(isinf(color)))
    {
        color = float3(1, 0, 1);
    }
    
    // --------------------ポストプロセス--------------------// 
    // ACES トーンマッピング
    float3 aces_input = color * 0.6f;
    float3 a = aces_input * (aces_input + 0.0245786f) - 0.000090537f;
    float3 b = aces_input * (0.983729f * aces_input + 0.4329510f) + 0.238081f;
    color = a / b;
    
    // カラーマトリックス変換
    float3x3 colorMatrix = float3x3(
        1.0478112f, 0.0228866f, -0.0501270f,
        -0.0295081f, 0.9904844f, 0.0150436f,
        -0.0092345f, 0.0150436f, 0.7521316f
    );
    color = mul(colorMatrix, color);
    
    // sRGB ガンマ補正
    float3 srgb;
    srgb.x = (color.x <= 0.0031308f) ? color.x * 12.92f : 1.055f * pow(color.x, 1.0f / 2.4f) - 0.055f;
    srgb.y = (color.y <= 0.0031308f) ? color.y * 12.92f : 1.055f * pow(color.y, 1.0f / 2.4f) - 0.055f;
    srgb.z = (color.z <= 0.0031308f) ? color.z * 12.92f : 1.055f * pow(color.z, 1.0f / 2.4f) - 0.055f;
    color = srgb;
    
    // G-buffer出力の値検証
    if (any(isnan(finalAlbedo)) || any(isinf(finalAlbedo)))
        finalAlbedo = float3(0, 0, 0);
    if (any(isnan(finalNormal)) || any(isinf(finalNormal)))
        finalNormal = float3(0, 0, 1);
    if (isnan(finalDepth) || isinf(finalDepth))
        finalDepth = 0.0f;
    
    
    // --------------------テンポラルアキュムレーション--------------------//
    // フレームのジオメトリ情報をエンコード
    float4 prevAccumulation = AccumulationBuffer[launchIndex.xy];
    float frameCount = prevAccumulation.a;
    
    // カメラ移動の強制リセット判定
    bool forceReset = (cameraMovedFlag != 0u);
    bool isFirstFrame = (frameCount < 0.5f) || forceReset;
    
    float3 accumulatedColor;
    float newFrameCount;
    
    if (isFirstFrame)
    {
        // 初回フレームまたはリセット時
        accumulatedColor = color;
        newFrameCount = 1.0f;
    }
    else
    {
        // 前フレームとの類似性判定
        float4 prevFrameData = PrevFrameData[launchIndex.xy];
        
        float normalSimilarity = dot(finalNormal, prevFrameData.rgb);
        
        float depthDiff = abs(finalDepth - prevFrameData.a);
        float depthSimilarity = exp(-depthDiff);
        
        float similarity = normalSimilarity * 0.7f + depthSimilarity * 0.3f;
        
        bool shouldAccumulate = (similarity > 0.9f);
        
        // 類似性が高い場合は累積を継続
        if (shouldAccumulate)
        {
            float3 prevColor = prevAccumulation.rgb;
            newFrameCount = frameCount + 1.0f;
            
            // フレーム数に応じた適応的ブレンド率
            float alpha;
            if (newFrameCount < 8.0f)
            {
                alpha = 1.0f / newFrameCount;
            }
            else if (newFrameCount < 32.0f)
            {
                alpha = 0.1f;
            }
            else if (newFrameCount < 64.0f)
            {
                alpha = 0.05f;
            }
            else
            {
                alpha = 0.02f;
                newFrameCount = 64.0f;
            }
            
            accumulatedColor = lerp(prevColor, color, alpha);
        }
        else
        {
            // 類似性が低い場合はリセット
            accumulatedColor = color;
            newFrameCount = 1.0f;
        }
    }
    
    const float MAX_VIEW_DEPTH = 10.0f;
    
    // 各バッファへの出力
    RenderTarget[launchIndex.xy] = float4(accumulatedColor, 1.0f);
    AccumulationBuffer[launchIndex.xy] = float4(accumulatedColor, newFrameCount);
    PrevFrameData[launchIndex.xy] = float4(finalNormal, finalDepth);
    AlbedoOutput[launchIndex.xy] = float4(finalAlbedo, finalRoughness);
    NormalOutput[launchIndex.xy] = float4(finalNormal, 1.0f);
    DepthOutput[launchIndex.xy] = float4(finalDepth / MAX_VIEW_DEPTH, finalDepth / MAX_VIEW_DEPTH, finalDepth / MAX_VIEW_DEPTH, 1.0f);
}