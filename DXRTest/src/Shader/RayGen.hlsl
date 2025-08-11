#include "Common.hlsli"
#include "ReSTIR_GI.hlsli"
#include "ReSTIR_DI.hlsli"

// **Blue Noise Pattern 8×8 (縦線ノイズ除去用)**
static const float BlueNoise8x8[64] = {
    0.515625f, 0.140625f, 0.890625f, 0.328125f, 0.484375f, 0.171875f, 0.921875f, 0.359375f,
    0.015625f, 0.765625f, 0.265625f, 0.703125f, 0.046875f, 0.796875f, 0.296875f, 0.734375f,
    0.640625f, 0.078125f, 0.828125f, 0.203125f, 0.671875f, 0.109375f, 0.859375f, 0.234375f,
    0.390625f, 0.953125f, 0.453125f, 0.578125f, 0.421875f, 0.984375f, 0.484375f, 0.609375f,
    0.546875f, 0.125000f, 0.859375f, 0.281250f, 0.515625f, 0.156250f, 0.890625f, 0.312500f,
    0.078125f, 0.734375f, 0.234375f, 0.656250f, 0.109375f, 0.765625f, 0.265625f, 0.687500f,
    0.703125f, 0.031250f, 0.796875f, 0.156250f, 0.734375f, 0.062500f, 0.828125f, 0.187500f,
    0.343750f, 0.906250f, 0.406250f, 0.531250f, 0.375000f, 0.937500f, 0.437500f, 0.562500f
};


// **改良されたシード生成関数（Blue Noise Pattern使用）**
uint GenerateBlueNoiseSeed(uint2 pixelCoord, uint frameIndex, uint sampleIndex)
{
    // Blue Noise Patternからベース値を取得
    uint2 noiseCoord = pixelCoord & 7; // 8×8パターンでタイル
    uint noiseIndex = noiseCoord.y * 8 + noiseCoord.x;
    float blueNoiseValue = BlueNoise8x8[noiseIndex];
    
    // Blue Noise値を整数に変換
    uint blueNoiseSeed = uint(blueNoiseValue * 4294967295.0f); // 32bit max
    
    // **最適化: PCGHash使用でパフォーマンス重視**
    uint seed = blueNoiseSeed;
    seed ^= PCGHash(pixelCoord.x * 73856093u);  // 大きな素数
    seed ^= PCGHash(pixelCoord.y * 19349663u);  // 大きな素数
    seed ^= PCGHash(frameIndex * 83492791u);    // フレーム変動用
    seed ^= PCGHash(sampleIndex * 51726139u);   // サンプル変動用
    
    return PCGHash(seed);
}

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    // アンチエイリアシング用マルチサンプリング
    float3 totalColor = float3(0, 0, 0);
    float3 finalColor = float3(0, 0, 0);
    float3 finalAlbedo = float3(0, 0, 0);
    float3 finalNormal = float3(0, 0, 0);
    float finalDepth = 0.0f;
    uint finalMaterialType = 0;
    float finalRoughness = 0.0f;
    
    const int SAMPLES = 1; // パフォーマンス重視
    
    for (int sampleIndex = 0; sampleIndex < SAMPLES; sampleIndex++)
    {
        // **フレーム毎の乱数変動**
        uint frameIndex = uint(frameCount) % 64u;
        uint seed = GenerateBlueNoiseSeed(launchIndex.xy, frameIndex, sampleIndex);
        
        // アンチエイリアシング用ジッター
        float2 jitter = float2(RandomFloat(seed), RandomFloat(seed)) - 0.5f;
        float2 crd = float2(launchIndex.xy) + jitter;
        float2 dims = float2(launchDim.xy);
        
        // 正規化座標に変換 (-1 to 1)
        float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
        d.y = -d.y; // Y軸反転
        
        // カメラ位置
        float3 cameraPos = viewMatrix._m03_m13_m23;

        // レイ方向計算
        float3 rayDir = normalize(cameraForward +
                        d.x * cameraRight * tanHalfFov * aspectRatio +
                        d.y * cameraUp * tanHalfFov);
        
        // レイ設定
        RayDesc ray;
        ray.Origin = cameraPos;
        ray.Direction = rayDir;
        ray.TMin = 0.1f;
        ray.TMax = 10000.0f;
        
        // レイペイロード初期化
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
        


        float3 sampleColor = float3(0, 0, 0);
        
        // ヒットした場合のみ照明計算を実行
        if (payload.hitDistance > 0.0f && length(payload.worldPos) > 0.1f) {
            // G-Bufferから基本データ取得
            float3 worldPos = payload.worldPos;
            float3 normal = payload.normal;
            float3 albedo = payload.albedo;
            uint materialType = payload.materialType;
            float roughness = payload.roughness;
            
            // **デバッグ用：まずマテリアル基本情報でテスト**
            if (length(albedo) > 0.0f) {
                // **デバッグ：床の中央アーティファクト調査**
                // 床かどうかの判定（Y座標が低い位置）
                bool isFloor = (worldPos.y < -1.0f);
                
                // 床の中央部分かどうかの判定
                float2 floorCenter = float2(0.0f, 0.0f); // 仮の床中央
                float2 floorPos = worldPos.xz;
                float distanceFromCenter = length(floorPos - floorCenter);
                bool isFloorCenter = (isFloor && distanceFromCenter < 50.0f);
                // 通常のアルベド表示（壁など）
                float gray = dot(albedo, float3(0.3f, 0.6f, 0.1f));
                sampleColor = float3(gray, gray, gray);

                // **ReSTIR DI完全実装（別ファイルから呼び出し）**

                // マテリアルデータ構築
                MaterialData material;
                material.albedo = albedo;
                material.roughness = roughness;
                material.materialType = materialType;
                material.refractiveIndex = 1.0f;
                material.emission = float3(0, 0, 0);

                // スクリーン情報とReservoirインデックス
                uint2 screenDim = DispatchRaysDimensions().xy;
                uint reservoirIndex = GetReservoirIndex(launchIndex.xy, screenDim);

                // **ReSTIR DI統合実装を呼び出し**
                float3 directIllumination = CalculateReSTIRDI(worldPos, normal, material,
                    launchIndex.xy, screenDim, reservoirIndex, seed);

                // **ReSTIR GI完全実装**
                float3 indirectIllumination = CalculateReSTIRGI(worldPos, normal, material.albedo,
                    material.materialType, launchIndex.xy, screenDim, seed);

                // **ReSTIR DI + GI統合最適化**

                // **エネルギー正規化と品質バランス**
                float directLuminance = dot(directIllumination, float3(0.2126f, 0.7152f, 0.0722f));
                float indirectLuminance = dot(indirectIllumination, float3(0.2126f, 0.7152f, 0.0722f));
                float totalLuminance = directLuminance + indirectLuminance;

                // **間接照明を通常レベルで表示**
                // indirectIllumination = indirectIllumination * 1.0f; // 通常レベル

                // **統合照明: ReSTIR DI + GI**
                sampleColor = directIllumination + indirectIllumination;

                // **エネルギー制限**
                float finalLuminance = dot(sampleColor, float3(0.2126f, 0.7152f, 0.0722f));
                if ( finalLuminance > 3.0f ) {
                    // 過剰な明るさを制限
                    sampleColor = sampleColor * ( 3.0f / finalLuminance );
                }

                // **物理的環境光**（最小限）
                sampleColor += material.albedo * 0.001f; // 物理的に正確な最小環境光
            } else {
                // アルベド情報なし（マゼンタでエラー表示）
                sampleColor = float3(1.0f, 0.0f, 1.0f);
            }
        } else {
            // ヒットしなかった場合（背景）
            sampleColor = payload.color;
        }
        
        // サンプル結果を累積
        totalColor += sampleColor;
        
        // プライマリレイのG-Bufferデータのみ保存
        if (sampleIndex == 0) {
            finalAlbedo = payload.albedo;
            finalNormal = payload.normal;
            finalDepth = payload.hitDistance;
            finalMaterialType = payload.materialType;
            finalRoughness = payload.roughness;
        }
    }
    
    // サンプル平均化
    finalColor = totalColor / float(SAMPLES);
    
    // 法線の正規化
    if (length(finalNormal) > 0.1f)
    {
        finalNormal = normalize(finalNormal);
    }
    else
    {
        finalNormal = float3(0, 0, 1);
    }
    
    // ポストプロセシング
    float3 color = finalColor;
    
    // NaNチェック
    if (any(isnan(color)) || any(isinf(color)))
    {
        color = float3(1, 0, 1); // マゼンタでエラー表示
    }
    
    // NaN/Inf以外は元の色を保持（デバッグ表示削除）
    
    // **物理的に正確なACESトーンマッピング**
    float3 aces_input = color * 0.6f;
    float3 a = aces_input * (aces_input + 0.0245786f) - 0.000090537f;
    float3 b = aces_input * (0.983729f * aces_input + 0.4329510f) + 0.238081f;
    color = a / b;
    
    // **色温度調整**
    float3x3 colorMatrix = float3x3(
        1.0478112f,  0.0228866f, -0.0501270f,
        -0.0295081f, 0.9904844f,  0.0150436f,
        -0.0092345f, 0.0150436f,  0.7521316f
    );
    color = mul(colorMatrix, color);
    
    // **適応的露出補正**
    float avgLuminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float targetLuminance = 0.18f;
    float exposureAdjust = targetLuminance / max(avgLuminance, 0.001f);
    exposureAdjust = clamp(exposureAdjust, 0.5f, 2.0f);
    color *= exposureAdjust;
    
    // **正確なsRGBガンマ補正**
    float3 srgb;
    srgb.x = (color.x <= 0.0031308f) ? color.x * 12.92f : 1.055f * pow(color.x, 1.0f/2.4f) - 0.055f;
    srgb.y = (color.y <= 0.0031308f) ? color.y * 12.92f : 1.055f * pow(color.y, 1.0f/2.4f) - 0.055f;
    srgb.z = (color.z <= 0.0031308f) ? color.z * 12.92f : 1.055f * pow(color.z, 1.0f/2.4f) - 0.055f;
    color = srgb;
    
    // G-Bufferのガンマ補正とNaNチェック
    if (any(isnan(finalAlbedo)) || any(isinf(finalAlbedo)))
        finalAlbedo = float3(0, 0, 0);
    if (any(isnan(finalNormal)) || any(isinf(finalNormal)))
        finalNormal = float3(0, 0, 1);
    if (isnan(finalDepth) || isinf(finalDepth))
        finalDepth = 0.0f;
    
    // 深度の正規化
    const float MAX_VIEW_DEPTH = 10.0f;
    float normalizedDepth = saturate(finalDepth / MAX_VIEW_DEPTH);

    // **時間的蓄積処理**
    float3 normalizedNormal = finalNormal * 0.5f + 0.5f;
    float4 currentFrameData = float4(normalizedNormal, normalizedDepth);
    float4 prevAccumulation = AccumulationBuffer[launchIndex.xy];
    float frameCount = prevAccumulation.a;
    
    // **カメラ動き検出による強制リセット**
    bool forceReset = (cameraMovedFlag != 0u);
    
    // **初期フレーム検出**
    bool isFirstFrame = (frameCount < 0.5f) || forceReset;
    
    float3 accumulatedColor;
    float newFrameCount;
    
    if (isFirstFrame) {
        accumulatedColor = color;
        newFrameCount = 1.0f;
    } else {
        float4 prevFrameData = PrevFrameData[launchIndex.xy];
        
        float3 currentNormal = currentFrameData.rgb * 2.0f - 1.0f;
        float3 prevNormal = prevFrameData.rgb * 2.0f - 1.0f;
        
        float normalSimilarity = dot(currentNormal, prevNormal);
        float depthDiff = abs(currentFrameData.a - prevFrameData.a);
        
        float depthSimilarity = exp(-depthDiff * 10.0f);
        float similarity = normalSimilarity * 0.7f + depthSimilarity * 0.3f;
        
        bool shouldAccumulate = (similarity > 0.9f);
        
        if (shouldAccumulate) {
            float3 prevColor = prevAccumulation.rgb;
            newFrameCount = frameCount + 1.0f;
            
            float alpha;
            if (newFrameCount < 8.0f) {
                alpha = 1.0f / newFrameCount;
            } else if (newFrameCount < 32.0f) {
                alpha = 0.1f;
            } else if (newFrameCount < 64.0f) {
                alpha = 0.05f;
            } else {
                alpha = 0.02f;
                newFrameCount = 64.0f;
            }
            
            accumulatedColor = lerp(prevColor, color, alpha);
        } else {
            accumulatedColor = color;
            newFrameCount = 1.0f;
        }
    }
    
    // 結果を出力
    RenderTarget[launchIndex.xy] = float4(accumulatedColor, 1.0f);
    AccumulationBuffer[launchIndex.xy] = float4(accumulatedColor, newFrameCount);
    PrevFrameData[launchIndex.xy] = currentFrameData;
    AlbedoOutput[launchIndex.xy] = float4(finalAlbedo, finalRoughness);
    NormalOutput[launchIndex.xy] = float4(finalNormal, 1.0f);
    DepthOutput[launchIndex.xy] = float4(normalizedDepth, normalizedDepth, normalizedDepth, 1.0f);
}