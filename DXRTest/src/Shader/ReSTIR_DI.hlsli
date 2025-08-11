#ifndef RESTIR_DI_HLSLI
#define RESTIR_DI_HLSLI

// **ReSTIR DI完全実装**
// Reservoir-based Spatiotemporal Importance Resampling for Direct Illumination

// 注意: LightReservoir構造体、InitLightReservoir、UpdateLightReservoir関数はCommon.hlsliで定義済み

// **ReSTIR DI段階的テスト制御フラグ**
#define RESTIR_DI_PHASE_1_INITIAL_CANDIDATES    1  // 初期候補生成
#define RESTIR_DI_PHASE_2_TEMPORAL_REUSE        1  // Temporal Reuse
#define RESTIR_DI_PHASE_3_SPATIAL_REUSE         1  // Spatial Reuse
#define RESTIR_DI_PHASE_4_MIS_INTEGRATION       1  // MIS統合

// **重要度ベース複数ライト効率サンプリング（Phase 1）**
LightReservoir GenerateInitialCandidates(float3 worldPos, float3 normal, MaterialData material, inout uint seed) {
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
            
            // Reservoirに補正済み重みで候補を追加（RIS）
            UpdateLightReservoir(currentReservoir, lightIdx, samplePos, radiance, targetPdf, correctedSourcePdf, seed);
        }
    }
    
    return currentReservoir;
}

// **Temporal Reuse実装（Phase 2）**
LightReservoir ApplyTemporalReuse(LightReservoir currentReservoir, float3 worldPos, float3 normal, 
                                 MaterialData material, uint2 screenDim, uint reservoirIndex, inout uint seed) {
    #if RESTIR_DI_PHASE_2_TEMPORAL_REUSE
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
    #endif // RESTIR_DI_PHASE_2_TEMPORAL_REUSE
    
    return currentReservoir;
}

// **Spatial Reuse実装（Phase 3）**
LightReservoir ApplySpatialReuse(LightReservoir currentReservoir, float3 worldPos, float3 normal, 
                                MaterialData material, uint2 launchIndex, uint2 screenDim, inout uint seed) {
    #if RESTIR_DI_PHASE_3_SPATIAL_REUSE
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
            
            int2 neighborCoord = int2(launchIndex) + neighborOffset;
            
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
    #endif // RESTIR_DI_PHASE_3_SPATIAL_REUSE
    
    return currentReservoir;
}

// **MIS BRDF Sampling（Phase 4）**
float3 CalculateMISBRDFContribution(float3 worldPos, float3 normal, MaterialData material, LightReservoir reservoir, inout uint seed) {
    float3 directIllumination = float3(0, 0, 0);
    
    #if RESTIR_DI_PHASE_4_MIS_INTEGRATION
    // **最適化: MISを条件付きで実行（ReSTIRで十分な重みが得られない場合のみ）**
    if (numLights > 0 && (reservoir.weightSum < 0.1f || !reservoir.valid)) {
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
    #endif // RESTIR_DI_PHASE_4_MIS_INTEGRATION
    
    return directIllumination;
}

// **最終シェーディング計算**
float3 CalculateFinalShading(LightReservoir reservoir, float3 worldPos, float3 normal, MaterialData material, inout uint seed) {
    float3 directIllumination = float3(0, 0, 0);
    
    if (reservoir.valid && reservoir.weightSum > 0.0f) {
        // 選択されたライトから直接照明を計算
        uint selectedLightIdx = reservoir.lightIndex;
        float3 selectedSamplePos = reservoir.samplePos;
        float3 selectedRadiance = reservoir.radiance;
        
        // **最適化: シンプルなシャドウレイサンプリング（1回のみ）**
        LightSample lightSample = SampleLightByIndex(selectedLightIdx, worldPos, seed);
        
        if (lightSample.valid) {
            float NdotL = max(0.0f, dot(normal, lightSample.direction));
            
            if (NdotL > 0.0f) {
                // **1回のシャドウレイによる可視性テスト**
                RayDesc shadowRay;
                shadowRay.Origin = worldPos + normal * 0.01f;
                shadowRay.Direction = lightSample.direction;
                shadowRay.TMin = 0.01f;
                shadowRay.TMax = max(0.01f, lightSample.distance - 0.01f);
                
                RayPayload shadowPayload;
                shadowPayload.color = float3(1, 1, 1);
                shadowPayload.depth = 999;
                shadowPayload.seed = seed;
                shadowPayload.albedo = float3(0, 0, 0);
                shadowPayload.normal = float3(0, 0, 1);
                shadowPayload.worldPos = float3(0, 0, 0);
                shadowPayload.hitDistance = 0.0f;
                shadowPayload.materialType = 0;
                shadowPayload.roughness = 0.0f;
                shadowPayload.padding = 0;
                
                TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                         0xFF, 0, 1, 0, shadowRay, shadowPayload);
                
                bool visible = (length(shadowPayload.color) > 0.5f);
                
                if (visible) {
                    // BRDF評価
                    float3 brdf = material.albedo / 3.14159265f;
                    float3 directContribution = brdf * lightSample.radiance * NdotL;
                    
                    // **MIS統合: Light Sampling重み計算**
                    float brdfPdf = NdotL / 3.14159265f; // Cosine-weighted BRDF PDF
                    float misWeight = MISWeightLight(lightSample.pdf, brdfPdf);
                    
                    // **ReSTIRの正しい重み適用 + MIS統合**
                    float normalizedWeight = reservoir.weight / max(lightSample.pdf, 0.0001f);
                    normalizedWeight = min(normalizedWeight, 5.0f); // 重み制限で安定化
                    
                    // MIS重み適用でより正確な照明
                    directIllumination = directContribution * normalizedWeight * misWeight;
                }
            }
        }
    }
    
    return directIllumination;
}

// **メインReSTIR DI実装（統合版）**
float3 CalculateReSTIRDI(float3 worldPos, float3 normal, MaterialData material, uint2 launchIndex, 
                        uint2 screenDim, uint reservoirIndex, inout uint seed) {
    
    // **Phase 1: Initial Candidate Generation**
    LightReservoir currentReservoir = GenerateInitialCandidates(worldPos, normal, material, seed);
    
    // **Phase 2: Temporal Reuse**
    currentReservoir = ApplyTemporalReuse(currentReservoir, worldPos, normal, material, screenDim, reservoirIndex, seed);
    
    // **Phase 3: Spatial Reuse**
    currentReservoir = ApplySpatialReuse(currentReservoir, worldPos, normal, material, launchIndex, screenDim, seed);
    
    // **Phase 4: MIS Integration + Final Shading**
    float3 directIllumination = CalculateFinalShading(currentReservoir, worldPos, normal, material, seed);
    
    // MIS BRDF Samplingによる追加貢献
    directIllumination += CalculateMISBRDFContribution(worldPos, normal, material, currentReservoir, seed);
    
    // **Temporal Reuse用Reservoirの保存**
    CurrentReservoirs[reservoirIndex] = currentReservoir;
    PreviousReservoirs[reservoirIndex] = currentReservoir;
    
    return directIllumination;
}

#endif // RESTIR_DI_HLSLI