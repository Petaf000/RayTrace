#include "Common.hlsli"

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

// **MIS用: Cosine-weighted Hemisphere Sampling**
float3 SampleCosineHemisphere(float3 normal, inout uint seed)
{
    float u1 = RandomFloat(seed);
    float u2 = RandomFloat(seed);
    
    // Cosine-weighted sampling
    float cosTheta = sqrt(u1);
    float sinTheta = sqrt(1.0f - u1);
    float phi = 2.0f * 3.14159265f * u2;
    
    float3 sample = float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));
    
    // Convert to world space aligned with normal
    float3 up = abs(normal.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    return sample.x * tangent + sample.y * normal + sample.z * bitangent;
}

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
        
        // **簡易照明テスト（ReSTIR DI実装前のテスト）**
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
                
                // **デバッグモード切り替え**
                bool materialDebugMode = false; // デバッグOFF - Phase 2進行のため
                
                if (materialDebugMode) {
                    // **アーティファクト詳細情報デバッグ**
                    
                    // 疑わしいアルベド値をチェック
                    float3 suspectAlbedo = float3(121.0f/255.0f, 119.0f/255.0f, 105.0f/255.0f);
                    float albedoSimilarity = distance(albedo, suspectAlbedo);
                    
                    if (albedoSimilarity < 0.05f) {
                        // **アーティファクト発見！アルベドとマテリアル情報で分析**
                        
                        // アルベドの具体的な値をRGB成分として表示
                        sampleColor = float3(1.0f, 0.0f, 0.0f); // 基本は赤色
                        
                        // アルベドの細かい値による識別
                        if (albedo.r > albedo.g && albedo.r > albedo.b) {
                            // 赤成分が最大 → 赤系のマテリアル
                            sampleColor = float3(1.0f, 0.0f, 0.0f);
                        } else if (albedo.g > albedo.r && albedo.g > albedo.b) {
                            // 緑成分が最大 → 緑系のマテリアル
                            sampleColor = float3(0.0f, 1.0f, 0.0f);
                        } else if (albedo.b > albedo.r && albedo.b > albedo.g) {
                            // 青成分が最大 → 青系のマテリアル
                            sampleColor = float3(0.0f, 0.0f, 1.0f);
                        } else {
                            // ほぼ同じ → グレー系
                            sampleColor = float3(0.5f, 0.5f, 0.5f);
                        }
                        
                        // マテリアルタイプによる追加識別
                        if (materialType == 0) {
                            sampleColor += float3(0.2f, 0.0f, 0.0f); // Lambertian: 赤み追加
                        } else if (materialType == 1) {
                            sampleColor += float3(0.0f, 0.2f, 0.0f); // Metal: 緑み追加
                        } else if (materialType == 2) {
                            sampleColor += float3(0.0f, 0.0f, 0.2f); // Dielectric: 青み追加
                        } else if (materialType == 3) {
                            sampleColor = float3(1.0f, 0.0f, 1.0f); // DiffuseLight: 固定マゼンタ
                        }
                        
                        sampleColor = saturate(sampleColor); // 0-1範囲に制限
                    } else {
                        // 通常オブジェクト：詳細座標デバッグ
                        float2 centerDist2D = worldPos.xz;
                        float distFromCenter = length(centerDist2D);
                        
                        // より細かい距離判定（Cornell Boxの実際のサイズに合わせる）
                        if (distFromCenter < 25.0f) {
                            sampleColor = float3(1.0f, 0.0f, 0.0f); // 赤：中心25unit以内
                        } else if (distFromCenter < 50.0f) {
                            sampleColor = float3(1.0f, 1.0f, 0.0f); // 黄：25-50unit
                        } else if (distFromCenter < 100.0f) {
                            sampleColor = float3(0.0f, 1.0f, 0.0f); // 緑：50-100unit
                        } else {
                            sampleColor = float3(0.0f, 0.0f, 1.0f); // 青：100unit超
                        }
                        
                        // Y座標による明度調整（高さ情報を反映）
                        float heightFactor = (worldPos.y + 100.0f) / 200.0f; // -100~100を0~1に変換
                        heightFactor = saturate(heightFactor);
                        sampleColor *= (0.3f + heightFactor * 0.7f); // 30%-100%の明度
                        
                        // マテリアルタイプ情報を混合
                        if (materialType == 3) {
                            sampleColor = float3(1.0f, 0.0f, 1.0f); // マゼンタ：DiffuseLight
                        }
                    }
                    // **デバッグモードでは照明計算をスキップ**
                } else {
                    // 通常のアルベド表示（壁など）
                    float gray = dot(albedo, float3(0.3f, 0.6f, 0.1f));
                    sampleColor = float3(gray, gray, gray);
                    
                    // **Phase 2: 真のReSTIR DI実装開始**
                    
                    // マテリアルデータ構築
                    MaterialData material;
                    material.albedo = albedo;
                    material.roughness = roughness;
                    material.materialType = materialType;
                    material.refractiveIndex = 1.0f;
                    material.emission = float3(0, 0, 0);
                    
                    // **ReSTIR DI: 初期候補生成（Phase 2.1）**
                    uint2 screenDim = DispatchRaysDimensions().xy;
                    uint reservoirIndex = GetReservoirIndex(launchIndex.xy, screenDim);
                    
                    // **Phase 2.5: 重要度ベース複数ライト効率サンプリング**
                    const int INITIAL_CANDIDATES = min(4, int(numLights)); // 最大4候補
                    LightReservoir currentReservoir = InitLightReservoir();
                    
                    // **Step 1: 全ライトの重要度事前計算**
                    float lightImportances[8]; // 最大8ライト対応
                    float totalImportance = 0.0f;
                    
                    for (uint preIdx = 0; preIdx < min(numLights, 8u); preIdx++) {
                        LightInfo preLight = GetLightByIndex(preIdx);
                        float3 toLightCenter = preLight.position - worldPos;
                        float distToCenter = length(toLightCenter);
                        float3 lightDirCenter = normalize(toLightCenter);
                        float NdotL_estimate = max(0.0f, dot(normal, lightDirCenter));
                        
                        // 重要度 = 輝度 × コサイン × 距離減衰 × ライト面積
                        float luminance = dot(preLight.emission, float3(0.2126f, 0.7152f, 0.0722f));
                        float attenuation = 1.0f / max(distToCenter * distToCenter, 1.0f);
                        float area = preLight.area;
                        
                        lightImportances[preIdx] = luminance * NdotL_estimate * attenuation * area;
                        totalImportance += lightImportances[preIdx];
                    }
                    
                    // **Step 2: 重要度サンプリングによる候補選択**
                    for (int candidateIdx = 0; candidateIdx < INITIAL_CANDIDATES; candidateIdx++) {
                        uint lightIdx;
                        float lightSelectionPdf = 1.0f / float(numLights); // デフォルト均等
                        
                        if (totalImportance > 0.0001f && numLights > 1) {
                            // 重要度ベースサンプリング
                            float random = RandomFloat(seed);
                            float cumulativeProb = 0.0f;
                            
                            for (uint selectIdx = 0; selectIdx < min(numLights, 8u); selectIdx++) {
                                float probability = lightImportances[selectIdx] / totalImportance;
                                cumulativeProb += probability;
                                
                                if (random <= cumulativeProb || selectIdx == numLights - 1) {
                                    lightIdx = selectIdx;
                                    lightSelectionPdf = probability;
                                    break;
                                }
                            }
                        } else {
                            // フォールバック: ランダム選択
                            lightIdx = uint(RandomFloat(seed) * float(numLights)) % numLights;
                        }
                        LightInfo light = GetLightByIndex(lightIdx);
                        
                        // ライト表面でのサンプリング（修正版）
                        LightSample lightSample = SampleLightByIndex(lightIdx, worldPos, seed);
                        float sourcePdf = lightSample.pdf;
                        float3 samplePos = lightSample.position;
                        
                        // Target PDF計算（物理的重要度）
                        float3 lightDir = normalize(samplePos - worldPos);
                        float lightDist = length(samplePos - worldPos);
                        float NdotL = max(0.0f, dot(normal, lightDir));
                        
                        if (lightSample.valid && NdotL > 0.0f && sourcePdf > 0.0f) {
                            // Target PDF = BRDF * cosine * radiance * attenuation
                            float attenuation = 1.0f / (lightDist * lightDist);
                            float3 brdf = material.albedo / 3.14159265f;
                            float3 radiance = light.emission;
                            float luminance = dot(radiance, float3(0.2126f, 0.7152f, 0.0722f));
                            
                            float targetPdf = luminance * NdotL * attenuation;
                            
                            // **重要度サンプリング補正: より正確なunbiased estimator**
                            // 重み補正 = targetPdf / (sourcePdf × lightSelectionPdf)
                            float correctedSourcePdf = sourcePdf * lightSelectionPdf;
                            float unbiasedWeight = targetPdf / max(correctedSourcePdf, 0.0001f);
                            
                            // Reservoirに補正済み重みで候補を追加（RIS）
                            UpdateLightReservoir(currentReservoir, lightIdx, samplePos, radiance, targetPdf, correctedSourcePdf, seed);
                        }
                    }
                    
                    // **ReSTIR DI: Temporal Reuse（Phase 2.2）**
                    if (!cameraMovedFlag && reservoirIndex < (screenDim.x * screenDim.y)) {
                        // カメラが静止している場合、前フレームのReservoirを再利用
                        LightReservoir prevReservoir = PreviousReservoirs[reservoirIndex];
                        
                        if (prevReservoir.valid && prevReservoir.weightSum > 0.0f) {
                            // 前フレームのサンプルを現在のシェーディングポイントで再評価
                            LightInfo prevLight = GetLightByIndex(prevReservoir.lightIndex);
                            float3 prevSamplePos = prevReservoir.samplePos;
                            
                            // 現在のシェーディングポイントでのTarget PDF再計算
                            float3 lightDir = normalize(prevSamplePos - worldPos);
                            float lightDist = length(prevSamplePos - worldPos);
                            float NdotL = max(0.0f, dot(normal, lightDir));
                            
                            if (NdotL > 0.0f) {
                                // バイアス補正のためのTarget PDF再評価
                                float attenuation = 1.0f / (lightDist * lightDist);
                                float3 brdf = material.albedo / 3.14159265f;
                                float3 radiance = prevLight.emission;
                                float luminance = dot(radiance, float3(0.2126f, 0.7152f, 0.0722f));
                                
                                float recomputedTargetPdf = luminance * NdotL * attenuation;
                                
                                // **RTXDI準拠Temporal Reservoir結合（修正版）**
                                if (recomputedTargetPdf > 0.0f) {
                                    // 前フレームの重みを現在の条件で再評価（バイアス補正）
                                    float correctedWeight = recomputedTargetPdf / max(prevReservoir.pdf, 0.0001f);
                                    correctedWeight = min(correctedWeight, 10.0f); // 重み上限制限
                                    
                                    // M値制限（ReSTIR安定性のため）
                                    uint maxM = min(prevReservoir.sampleCount, 20u); // 最大M=20
                                    
                                    // 重み付きReservoir Sampling（RIS）
                                    float temporalContribution = correctedWeight * float(maxM);
                                    float totalWeight = currentReservoir.weightSum + temporalContribution;
                                    
                                    if (totalWeight > 0.0f) {
                                        // 前フレームサンプル採用確率
                                        float probability = temporalContribution / totalWeight;
                                        
                                        if (RandomFloat(seed) < probability) {
                                            // 前フレームのサンプルを採用
                                            currentReservoir.lightIndex = prevReservoir.lightIndex;
                                            currentReservoir.samplePos = prevReservoir.samplePos;
                                            currentReservoir.radiance = prevReservoir.radiance;
                                        }
                                        
                                        // 統合重み更新
                                        currentReservoir.weightSum = totalWeight;
                                        currentReservoir.sampleCount += maxM;
                                        
                                        // 最終重み正規化（一度だけ）
                                        currentReservoir.weight = totalWeight / float(currentReservoir.sampleCount);
                                        currentReservoir.pdf = recomputedTargetPdf;
                                    }
                                }
                            }
                        }
                    }
                    
                    // **ReSTIR DI: Spatial Reuse（Phase 2.3）**
                    if (currentReservoir.valid && currentReservoir.weightSum > 0.0f) {
                        // 近隣ピクセルからのReservoirサンプリング
                        const int SPATIAL_SAMPLES = 4; // 4近隣サンプル
                        const float SPATIAL_RADIUS = 8.0f; // ピクセル単位の探索半径
                        
                        for (int spatialIdx = 0; spatialIdx < SPATIAL_SAMPLES; spatialIdx++) {
                            // ランダムな近隣位置生成（円形分布）
                            float angle = (float(spatialIdx) / float(SPATIAL_SAMPLES)) * 2.0f * 3.14159265f + RandomFloat(seed) * 1.0f;
                            float radius = sqrt(RandomFloat(seed)) * SPATIAL_RADIUS;
                            
                            int2 neighborOffset = int2(
                                radius * cos(angle),
                                radius * sin(angle)
                            );
                            
                            int2 neighborCoord = int2(launchIndex.xy) + neighborOffset;
                            
                            // スクリーン境界チェック
                            if (neighborCoord.x >= 0 && neighborCoord.y >= 0 && 
                                neighborCoord.x < int(screenDim.x) && neighborCoord.y < int(screenDim.y)) {
                                
                                uint neighborIndex = GetReservoirIndex(uint2(neighborCoord), screenDim);
                                
                                if (neighborIndex < (screenDim.x * screenDim.y)) {
                                    LightReservoir neighborReservoir = CurrentReservoirs[neighborIndex];
                                    
                                    if (neighborReservoir.valid && neighborReservoir.weightSum > 0.0f) {
                                        // 近隣サンプルを現在のシェーディングポイントで再評価
                                        LightInfo neighborLight = GetLightByIndex(neighborReservoir.lightIndex);
                                        float3 neighborSamplePos = neighborReservoir.samplePos;
                                        
                                        // 現在の位置での有効性チェック
                                        float3 lightDir = normalize(neighborSamplePos - worldPos);
                                        float lightDist = length(neighborSamplePos - worldPos);
                                        float NdotL = max(0.0f, dot(normal, lightDir));
                                        
                                        if (NdotL > 0.0f && lightDist > 0.001f) {
                                            // **Spatial Visibility Test（重要：異なる位置からの可視性確認）**
                                            RayDesc visibilityRay;
                                            visibilityRay.Origin = worldPos + normal * 0.01f;
                                            visibilityRay.Direction = lightDir;
                                            visibilityRay.TMin = 0.01f;
                                            visibilityRay.TMax = max(0.01f, lightDist - 0.01f);
                                            
                                            RayPayload visibilityPayload;
                                            visibilityPayload.color = float3(1, 1, 1);
                                            visibilityPayload.depth = 999;
                                            visibilityPayload.seed = seed + spatialIdx;
                                            visibilityPayload.albedo = float3(0, 0, 0);
                                            visibilityPayload.normal = float3(0, 0, 1);
                                            visibilityPayload.worldPos = float3(0, 0, 0);
                                            visibilityPayload.hitDistance = 0.0f;
                                            visibilityPayload.materialType = 0;
                                            visibilityPayload.roughness = 0.0f;
                                            visibilityPayload.padding = 0;
                                            
                                            TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                                                     0xFF, 0, 1, 0, visibilityRay, visibilityPayload);
                                            
                                            bool spatialVisible = (length(visibilityPayload.color) > 0.5f);
                                            
                                            if (spatialVisible) {
                                                // バイアス補正: Target PDF再計算
                                                float attenuation = 1.0f / (lightDist * lightDist);
                                                float3 brdf = material.albedo / 3.14159265f;
                                                float3 radiance = neighborLight.emission;
                                                float luminance = dot(radiance, float3(0.2126f, 0.7152f, 0.0722f));
                                                
                                                float spatialTargetPdf = luminance * NdotL * attenuation;
                                                
                                                if (spatialTargetPdf > 0.0f) {
                                                    // 重み補正（バイアス除去）
                                                    float correctedWeight = spatialTargetPdf / max(neighborReservoir.pdf, 0.0001f);
                                                    correctedWeight = min(correctedWeight, 8.0f); // Spatial用制限値
                                                    
                                                    // M値制限（Spatial安定性）
                                                    uint spatialM = min(neighborReservoir.sampleCount, 10u); // Spatialは控えめ
                                                    
                                                    // Spatial Reservoir結合
                                                    float spatialContribution = correctedWeight * float(spatialM);
                                                    float totalWeight = currentReservoir.weightSum + spatialContribution;
                                                    
                                                    if (totalWeight > 0.0f) {
                                                        float probability = spatialContribution / totalWeight;
                                                        
                                                        if (RandomFloat(seed) < probability) {
                                                            // 近隣サンプルを採用
                                                            currentReservoir.lightIndex = neighborReservoir.lightIndex;
                                                            currentReservoir.samplePos = neighborReservoir.samplePos;
                                                            currentReservoir.radiance = neighborReservoir.radiance;
                                                        }
                                                        
                                                        // 統合更新
                                                        currentReservoir.weightSum = totalWeight;
                                                        currentReservoir.sampleCount += spatialM;
                                                        currentReservoir.weight = totalWeight / float(currentReservoir.sampleCount);
                                                        currentReservoir.pdf = spatialTargetPdf;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // **Phase 2.4: MIS (Multiple Importance Sampling) 統合**
                    // ReSTIRとMISの組み合わせで最適な照明計算
                    float3 directIllumination = float3(0, 0, 0);
                    
                    // **MIS: BRDF Samplingによる追加貢献**
                    if (numLights > 0) {
                        // BRDF方向サンプリング（Cosine-weighted hemisphere）
                        float3 brdfSampleDir = SampleCosineHemisphere(normal, seed);
                        float brdfPdf = max(0.0f, dot(normal, brdfSampleDir)) / 3.14159265f;
                        
                        if (brdfPdf > 0.0f) {
                            // BRDF方向でのライトヒットテスト
                            RayDesc brdfRay;
                            brdfRay.Origin = worldPos + normal * 0.01f;
                            brdfRay.Direction = brdfSampleDir;
                            brdfRay.TMin = 0.01f;
                            brdfRay.TMax = 1000.0f;
                            
                            RayPayload brdfPayload;
                            brdfPayload.color = float3(0, 0, 0);
                            brdfPayload.depth = 0;
                            brdfPayload.seed = seed;
                            brdfPayload.albedo = float3(0, 0, 0);
                            brdfPayload.normal = float3(0, 0, 1);
                            brdfPayload.worldPos = float3(0, 0, 0);
                            brdfPayload.hitDistance = 0.0f;
                            brdfPayload.materialType = 0;
                            brdfPayload.roughness = 0.0f;
                            brdfPayload.padding = 0;
                            
                            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, brdfRay, brdfPayload);
                            
                            // ライトオブジェクトにヒットした場合
                            if (brdfPayload.hitDistance > 0.0f && brdfPayload.materialType == 3) { // DiffuseLight
                                float3 hitPos = brdfPayload.worldPos;
                                float hitDistance = brdfPayload.hitDistance;
                                
                                // ヒットしたライトのインデックス特定
                                for (uint lightIdx = 0; lightIdx < numLights; lightIdx++) {
                                    LightInfo testLight = GetLightByIndex(lightIdx);
                                    float3 lightCenter = testLight.position;
                                    
                                    // ライト範囲内かチェック
                                    float3 toLightCenter = hitPos - lightCenter;
                                    if (abs(toLightCenter.x) <= testLight.size.x * 0.5f && 
                                        abs(toLightCenter.z) <= testLight.size.z * 0.5f &&
                                        abs(toLightCenter.y) <= 0.1f) { // 薄いライト
                                        
                                        // MIS重み計算
                                        float lightPdf = CalculateLightPDF(lightIdx, worldPos, hitPos, brdfSampleDir);
                                        float misWeight = MISWeightBRDF(lightPdf, brdfPdf);
                                        
                                        if (misWeight > 0.0f) {
                                            // BRDF評価
                                            float3 brdf = material.albedo / 3.14159265f;
                                            float NdotL = max(0.0f, dot(normal, brdfSampleDir));
                                            float3 Le = testLight.emission;
                                            
                                            // MIS重み適用でバイアス除去
                                            float3 brdfContribution = (brdf * Le * NdotL * misWeight) / max(brdfPdf, 0.0001f);
                                            directIllumination += brdfContribution * 0.3f; // MIS貢献度調整
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    
                    // **ReSTIR DI: 最終照明計算（MIS統合版）**
                    
                    if (currentReservoir.valid && currentReservoir.weightSum > 0.0f) {
                        // 選択されたライトから直接照明を計算
                        uint selectedLightIdx = currentReservoir.lightIndex;
                        float3 selectedSamplePos = currentReservoir.samplePos;
                        float3 selectedRadiance = currentReservoir.radiance;
                        
                        // 高品質ソフトシャドウサンプリング（3x3 stratified）
                        const uint strataX = 3u;
                        const uint strataY = 3u;
                        const uint totalStrata = strataX * strataY;
                        float3 totalContribution = 0.0f;
                        
                        for (uint sy = 0; sy < strataY; sy++) {
                            for (uint sx = 0; sx < strataX; sx++) {
                                uint stratumSeed = seed + sy * strataX + sx;
                                
                                LightSample lightSample = SampleLightByIndexStratified(
                                    selectedLightIdx, worldPos, stratumSeed, sx, sy, strataX, strataY);
                                
                                if (lightSample.valid) {
                                    float NdotL = max(0.0f, dot(normal, lightSample.direction));
                                    
                                    if (NdotL > 0.0f) {
                                        // シャドウレイによる可視性テスト
                                        bool visible = true;
                                        
                                        RayDesc shadowRay;
                                        shadowRay.Origin = worldPos + normal * 0.01f;
                                        shadowRay.Direction = lightSample.direction;
                                        shadowRay.TMin = 0.01f;
                                        shadowRay.TMax = max(0.01f, lightSample.distance - 0.01f);
                                        
                                        RayPayload shadowPayload;
                                        shadowPayload.color = float3(1, 1, 1);
                                        shadowPayload.depth = 999;
                                        shadowPayload.seed = stratumSeed;
                                        shadowPayload.albedo = float3(0, 0, 0);
                                        shadowPayload.normal = float3(0, 0, 1);
                                        shadowPayload.worldPos = float3(0, 0, 0);
                                        shadowPayload.hitDistance = 0.0f;
                                        shadowPayload.materialType = 0;
                                        shadowPayload.roughness = 0.0f;
                                        shadowPayload.padding = 0;
                                        
                                        TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                                                 0xFF, 0, 1, 0, shadowRay, shadowPayload);
                                        
                                        visible = (length(shadowPayload.color) > 0.5f);
                                        
                                        if (visible) {
                                            // BRDF評価
                                            float3 brdf = material.albedo / 3.14159265f;
                                            float3 directContribution = brdf * lightSample.radiance * NdotL;
                                            
                                            // **MIS統合: Light Sampling重み計算**
                                            float brdfPdf = NdotL / 3.14159265f; // Cosine-weighted BRDF PDF
                                            float misWeight = MISWeightLight(lightSample.pdf, brdfPdf);
                                            
                                            // **ReSTIRの正しい重み適用 + MIS統合**
                                            float normalizedWeight = currentReservoir.weight / max(lightSample.pdf, 0.0001f);
                                            normalizedWeight = min(normalizedWeight, 5.0f); // 重み制限で安定化
                                            
                                            // MIS重み適用でより正確な照明
                                            float3 contribution = directContribution * normalizedWeight * misWeight;
                                            totalContribution += contribution;
                                        }
                                    }
                                }
                            }
                        }
                        
                        directIllumination = totalContribution / float(totalStrata);
                    }
                    
                    // **Temporal Reuse用Reservoirの保存**
                    // 現在のReservoirを保存（次フレームで前フレームとして使用）
                    CurrentReservoirs[reservoirIndex] = currentReservoir;
                    
                    // 前フレームのデータを更新（現在→前へのコピー）
                    PreviousReservoirs[reservoirIndex] = currentReservoir;
                    
                    // **GI Reservoir削除（メモリリーク対策）**
                    
                    // **Phase 3: ReSTIR GI実装（一時停止 - メモリリーク対策）**
                    float3 indirectIllumination = float3(0, 0, 0);
                    
                    // ReSTIR GI実装は一時停止中（メモリリーク問題のため）
                    // 将来的に安全な実装を追加予定
                    
                    // **Fallback: 従来GIパストレーシング（デバッグ用・ReSTIR GI無効時）**
                    bool useReSTIRGI = true; // ReSTIR GI使用フラグ
                    
                    if (!useReSTIRGI && material.materialType == 0) { // フォールバック実装
                        // **GI設定パラメータ**
                        const int GI_BOUNCES = 2; // 最大2バウンス
                        const float GI_CONTRIBUTION = 0.8f; // GI貢献度
                        
                        // **Russian Roulette設定**
                        float russianRouletteProb = 0.8f; // 継続確率
                        
                        // 現在のパストレーシング状態
                        float3 currentThroughput = material.albedo; // 初期反射率
                        float3 currentOrigin = worldPos;
                        float3 currentNormal = normal;
                        
                        for (int bounce = 1; bounce <= GI_BOUNCES; bounce++) {
                            // **Russian Roulette終了判定**
                            if (bounce > 1) {
                                float rr = RandomFloat(seed);
                                if (rr > russianRouletteProb) {
                                    break; // 確率的終了
                                }
                                // 継続する場合、throughputをRR確率で補正
                                currentThroughput /= russianRouletteProb;
                            }
                            
                            // **重要度サンプリング: Cosine-weighted hemisphere**
                            float3 giSampleDir = SampleCosineHemisphere(currentNormal, seed);
                            float giPdf = max(0.0f, dot(currentNormal, giSampleDir)) / 3.14159265f;
                            
                            if (giPdf > 0.0001f) {
                                // **GIレイ生成**
                                RayDesc giRay;
                                giRay.Origin = currentOrigin + currentNormal * 0.01f;
                                giRay.Direction = giSampleDir;
                                giRay.TMin = 0.01f;
                                giRay.TMax = 100.0f; // GI範囲制限
                                
                                RayPayload giPayload;
                                giPayload.color = float3(0, 0, 0);
                                giPayload.depth = bounce; // バウンス数記録
                                giPayload.seed = seed + bounce * 1000; // 異なるシード
                                giPayload.albedo = float3(0, 0, 0);
                                giPayload.normal = float3(0, 0, 1);
                                giPayload.worldPos = float3(0, 0, 0);
                                giPayload.hitDistance = 0.0f;
                                giPayload.materialType = 0;
                                giPayload.roughness = 0.0f;
                                giPayload.padding = 0;
                                
                                TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, giRay, giPayload);
                                
                                // **GIヒット処理**
                                if (giPayload.hitDistance > 0.0f && length(giPayload.worldPos) > 0.1f) {
                                    float3 giWorldPos = giPayload.worldPos;
                                    float3 giNormal = giPayload.normal;
                                    float3 giAlbedo = giPayload.albedo;
                                    uint giMaterialType = giPayload.materialType;
                                    
                                    // **ライトヒットチェック（直接照明寄与）**
                                    if (giMaterialType == 3) { // DiffuseLight
                                        // ライト直接ヒット: 直接照明として加算
                                        for (uint lightIdx = 0; lightIdx < numLights; lightIdx++) {
                                            LightInfo testLight = GetLightByIndex(lightIdx);
                                            float3 toLightCenter = giWorldPos - testLight.position;
                                            
                                            // ライト範囲内判定
                                            if (abs(toLightCenter.x) <= testLight.size.x * 0.5f && 
                                                abs(toLightCenter.z) <= testLight.size.z * 0.5f &&
                                                abs(toLightCenter.y) <= 0.1f) {
                                                
                                                float3 Le = testLight.emission;
                                                float NdotL = max(0.0f, dot(currentNormal, giSampleDir));
                                                
                                                // **GI寄与計算: BRDF × Le × cosine / PDF**
                                                float3 brdf = currentThroughput / 3.14159265f;
                                                float3 giContribution = (brdf * Le * NdotL) / giPdf;
                                                
                                                indirectIllumination += giContribution * GI_CONTRIBUTION;
                                                break;
                                            }
                                        }
                                        break; // ライトヒット時はパストレーシング終了
                                    }
                                    else if (giMaterialType == 0 && length(giAlbedo) > 0.0f) {
                                        // **Lambertianサーフェスヒット: 次バウンス準備**
                                        // 次バウンス用パラメータ更新
                                        currentThroughput *= giAlbedo; // アルベドによる減衰
                                        currentOrigin = giWorldPos;
                                        currentNormal = giNormal;
                                        
                                        // Throughputが小さくなりすぎた場合のRussian Roulette確率調整
                                        float luminance = dot(currentThroughput, float3(0.2126f, 0.7152f, 0.0722f));
                                        russianRouletteProb = min(0.95f, max(0.1f, luminance));
                                    }
                                    else {
                                        // その他のマテリアル: パストレーシング終了
                                        break;
                                    }
                                }
                                else {
                                    // Miss: 背景からの寄与（現在は0）
                                    break;
                                }
                            }
                        }
                    }
                    
                    // **Phase 3.3: ReSTIR DI + GI統合最適化**
                    
                    // **エネルギー正規化と品質バランス**
                    float directLuminance = dot(directIllumination, float3(0.2126f, 0.7152f, 0.0722f));
                    float indirectLuminance = dot(indirectIllumination, float3(0.2126f, 0.7152f, 0.0722f));
                    float totalLuminance = directLuminance + indirectLuminance;
                    
                    // **デバッグ: GI可視化モード**
                    bool debugGI = false; // GI無効化
                    
                    if (debugGI) {
                        // 間接照明のみ表示（10倍ブーストで可視化）
                        sampleColor = indirectIllumination * 10.0f;
                    } else {
                        // **段階的デバッグ: 直接照明のみテスト**
                        bool testDirectOnly = false; // 通常統合モード
                        
                        if (testDirectOnly) {
                            sampleColor = directIllumination;  // 直接照明のみ
                        } else {
                            sampleColor = directIllumination + indirectIllumination; // 完全統合
                        }
                    }
                    
                    // **通常統合: 直接照明のみ（GI一時無効化）**
                    sampleColor = directIllumination;
                    
                    // **エネルギー制限**
                    float finalLuminance = dot(sampleColor, float3(0.2126f, 0.7152f, 0.0722f));
                    if (finalLuminance > 3.0f) {
                        // 過剰な明るさを制限
                        sampleColor = sampleColor * (3.0f / finalLuminance);
                    }
                    
                    // **物理的環境光**（最小限）
                    sampleColor += material.albedo * 0.001f; // 物理的に正確な最小環境光
                } // materialDebugMode else の閉じ括弧
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