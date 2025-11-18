#include "../Common.hlsli"

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
    color = ACESToneMapping(color);
    color = ApplyColorMatrix(color);
    
    // sRGB ガンマ補正
    color = LinearToSRGB(color);
    
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