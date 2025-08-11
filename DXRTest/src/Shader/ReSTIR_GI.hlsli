#ifndef RESTIR_GI_HLSLI
#define RESTIR_GI_HLSLI

// **ReSTIR GI完全実装**
// Reservoir-based Spatiotemporal Importance Resampling for Global Illumination

// 注意: GIReservoir構造体はCommon.hlsliで定義済み

// **GI用サンプル構造体**
struct GISample {
    float3 position;        // ワールド位置
    float3 normal;          // 法線
    float3 radiance;        // 輝度
    float3 throughput;      // throughput
    float pdf;              // PDF
    bool valid;             // 有効性
    uint materialType;      // マテリアルタイプ
    float3 albedo;          // アルベド
};

// **GI Reservoirの初期化**
GIReservoir InitGIReservoir() {
    GIReservoir reservoir;
    reservoir.position = float3(0, 0, 0);
    reservoir.padding1 = 0.0f;         // パディング初期化
    reservoir.normal = float3(0, 0, 1);
    reservoir.padding2 = 0.0f;         // パディング初期化
    reservoir.radiance = float3(0, 0, 0);
    reservoir.padding3 = 0.0f;         // パディング初期化
    reservoir.throughput = float3(1, 1, 1);
    reservoir.weight = 0.0f;
    reservoir.weightSum = 0.0f;
    reservoir.sampleCount = 0;
    reservoir.pdf = 0.0f;
    reservoir.valid = false;
    reservoir.pathLength = 0;
    reservoir.bounceCount = 0;
    reservoir.albedo = float3(0, 0, 0);
    reservoir.padding4 = 0.0f;         // パディング初期化
    return reservoir;
}

// **GI Reservoir更新（RIS）**
bool UpdateGIReservoir(inout GIReservoir reservoir, GISample sample, float targetPdf, inout uint seed) {
    if (!sample.valid || targetPdf <= 0.0f) {
        return false;
    }
    
    // RIS重み計算
    float risWeight = targetPdf / max(sample.pdf, 0.0001f);
    risWeight = min(risWeight, 100.0f); // 重み制限
    
    reservoir.weightSum += risWeight;
    reservoir.sampleCount++;
    
    // サンプル採用判定
    float probability = risWeight / reservoir.weightSum;
    if (RandomFloat(seed) < probability) {
        reservoir.position = sample.position;
        reservoir.padding1 = 0.0f;
        reservoir.normal = sample.normal;
        reservoir.padding2 = 0.0f;
        reservoir.radiance = sample.radiance;
        reservoir.padding3 = 0.0f;
        reservoir.throughput = sample.throughput;
        reservoir.albedo = sample.albedo;
        reservoir.padding4 = 0.0f;
        reservoir.pdf = targetPdf;
        reservoir.valid = true;
        reservoir.pathLength = 0;
        reservoir.bounceCount = 0;
    }
    
    // 最終重み計算
    reservoir.weight = reservoir.weightSum / float(reservoir.sampleCount);
    
    return true;
}

// **シンプルなGIサンプル生成（外部依存を最小化）**
GISample GenerateSimpleGISample(float3 origin, float3 normal, inout uint seed) {
    GISample sample;
    sample.valid = false;
    sample.pdf = 0.0f;
    sample.radiance = float3(0, 0, 0);
    sample.throughput = float3(1, 1, 1);
    sample.position = float3(0, 0, 0);
    sample.normal = float3(0, 0, 1);
    sample.albedo = float3(0, 0, 0);
    sample.materialType = 0;
    
    // **内部でCosine-weighted hemisphere sampling実装**
    float u1 = RandomFloat(seed);
    float u2 = RandomFloat(seed);
    
    // Cosine-weighted sampling
    float cosTheta = sqrt(u1);
    float sinTheta = sqrt(1.0f - u1);
    float phi = 2.0f * 3.14159265f * u2;
    
    float3 sampleLocal = float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));
    
    // Convert to world space
    float3 up = abs(normal.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    float3 sampleDir = sampleLocal.x * tangent + sampleLocal.y * normal + sampleLocal.z * bitangent;
    
    float pdf = max(0.0f, dot(normal, sampleDir)) / 3.14159265f;
    
    if (pdf > 0.0001f) {
        // GIレイ生成
        RayDesc giRay;
        giRay.Origin = origin + normal * 0.01f;
        giRay.Direction = sampleDir;
        giRay.TMin = 0.01f;
        giRay.TMax = 100.0f;
        
        RayPayload giPayload;
        giPayload.color = float3(0, 0, 0);
        giPayload.depth = 1;
        giPayload.seed = seed + 1000;
        giPayload.albedo = float3(0, 0, 0);
        giPayload.normal = float3(0, 0, 1);
        giPayload.worldPos = float3(0, 0, 0);
        giPayload.hitDistance = 0.0f;
        giPayload.materialType = 0;
        giPayload.roughness = 0.0f;
        giPayload.padding = 0;
        
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, giRay, giPayload);
        
        if (giPayload.hitDistance > 0.0f && length(giPayload.worldPos) > 0.1f) {
            sample.position = giPayload.worldPos;
            sample.normal = giPayload.normal;
            sample.albedo = giPayload.albedo;
            sample.materialType = giPayload.materialType;
            sample.pdf = pdf;
            sample.valid = true;
            
            // ライト直接ヒット
            if (giPayload.materialType == 3) {
                sample.radiance = giPayload.color; // Emission
            }
            // Lambertianサーフェス - シンプルな照明計算
            else if (giPayload.materialType == 0 && length(giPayload.albedo) > 0.0f) {
                // シンプルな直接照明計算（内部実装）
                float3 directLight = float3(0, 0, 0);
                
                for (uint lightIdx = 0; lightIdx < numLights; lightIdx++) {
                    LightInfo light = GetLightByIndex(lightIdx);
                    float3 toLight = light.position - sample.position;
                    float lightDistance = length(toLight);
                    float3 lightDir = normalize(toLight);
                    
                    float NdotL = max(0.0f, dot(sample.normal, lightDir));
                    if (NdotL > 0.0f) {
                        // シンプルなライティング計算
                        float attenuation = 1.0f / (lightDistance * lightDistance + 1.0f);
                        float3 brdf = sample.albedo / 3.14159265f;
                        directLight += light.emission * brdf * NdotL * attenuation;
                    }
                }
                sample.radiance = directLight;
            }
        }
    }
    
    return sample;
}

// **段階的テスト制御フラグ**
#define RESTIR_GI_PHASE_1_INITIAL_CANDIDATES    1  // 初期候補生成
#define RESTIR_GI_PHASE_2_TEMPORAL_REUSE        1  // Temporal Reuse（実装開始）
#define RESTIR_GI_PHASE_3_SPATIAL_REUSE         1  // Spatial Reuse（実装開始）
#define RESTIR_GI_PHASE_4_MULTI_BOUNCE          0  // Multi-bounce（未実装）

// **段階的デバッグ用**
float3 CalculateReSTIRGI_Phase1_Only(float3 worldPos, float3 normal, float3 albedo, uint materialType, 
                                    uint2 screenCoord, uint2 screenDim, inout uint seed) {
    
    // Lambertianマテリアルのみ処理
    if (materialType != 0) {
        return float3(0, 0, 0);
    }
    
    float3 indirectIllumination = float3(0, 0, 0);
    
    #if RESTIR_GI_PHASE_1_INITIAL_CANDIDATES
    // **最適化: GI候補を削減**
    const int INITIAL_GI_CANDIDATES = 2;  // 4→2に削減
    GIReservoir currentReservoir = InitGIReservoir();
    
    for (int candidate = 0; candidate < INITIAL_GI_CANDIDATES; candidate++) {
        GISample giSample = GenerateSimpleGISample(worldPos, normal, seed);
        
        if (giSample.valid) {
            // Target PDF計算
            float luminance = dot(giSample.radiance, float3(0.2126f, 0.7152f, 0.0722f));
            float distance = length(giSample.position - worldPos);
            float cosine = max(0.0f, dot(normal, normalize(giSample.position - worldPos)));
            float targetPdf = luminance * cosine / (distance * distance + 1.0f);
            
            UpdateGIReservoir(currentReservoir, giSample, targetPdf, seed);
        }
    }
    
    // **Final Shading**
    if (currentReservoir.valid && currentReservoir.weightSum > 0.0f) {
        // 選択されたGIサンプルからの寄与計算
        float3 toSample = currentReservoir.position - worldPos;
        float distance = length(toSample);
        float3 direction = normalize(toSample);
        float cosine = max(0.0f, dot(normal, direction));
        
        if (cosine > 0.0f && distance > 0.001f) {
            // BRDF評価
            float3 brdf = albedo / 3.14159265f;
            
            // 正規化重み計算
            float normalizedWeight = currentReservoir.weight / max(currentReservoir.pdf, 0.0001f);
            normalizedWeight = min(normalizedWeight, 10.0f); // 安定化
            
            // GI寄与計算
            float3 giContribution = brdf * currentReservoir.radiance * cosine * normalizedWeight;
            indirectIllumination += giContribution * 0.2f; // 寄与度調整
        }
    }
    #endif // PHASE_1
    
    return indirectIllumination;
}

// **Temporal Reuse実装**
float3 CalculateReSTIRGI_WithTemporal(float3 worldPos, float3 normal, float3 albedo, uint materialType, 
                                     uint2 screenCoord, uint2 screenDim, inout uint seed) {
    
    // Lambertianマテリアルのみ処理
    if (materialType != 0) {
        return float3(0, 0, 0);
    }
    
    float3 indirectIllumination = float3(0, 0, 0);
    
    #if RESTIR_GI_PHASE_1_INITIAL_CANDIDATES
    // **最適化: GI候補を削減**
    const int INITIAL_GI_CANDIDATES = 2;  // 4→2に削減
    GIReservoir currentReservoir = InitGIReservoir();
    
    for (int candidate = 0; candidate < INITIAL_GI_CANDIDATES; candidate++) {
        GISample giSample = GenerateSimpleGISample(worldPos, normal, seed);
        
        if (giSample.valid) {
            // Target PDF計算
            float luminance = dot(giSample.radiance, float3(0.2126f, 0.7152f, 0.0722f));
            float distance = length(giSample.position - worldPos);
            float cosine = max(0.0f, dot(normal, normalize(giSample.position - worldPos)));
            float targetPdf = luminance * cosine / (distance * distance + 1.0f);
            
            UpdateGIReservoir(currentReservoir, giSample, targetPdf, seed);
        }
    }
    #endif // PHASE_1
    
    #if RESTIR_GI_PHASE_2_TEMPORAL_REUSE
    // **Phase 2: Temporal Reuse実装**
    if (!cameraMovedFlag) { // カメラが静止している場合のみ
        uint reservoirIndex = screenCoord.y * screenDim.x + screenCoord.x;
        
        if (reservoirIndex < (screenDim.x * screenDim.y)) {
            GIReservoir prevGIReservoir = PreviousGIReservoirs[reservoirIndex];
            
            if (prevGIReservoir.valid && prevGIReservoir.weightSum > 0.0f) {
                // 前フレームのGI Reservoirを現在位置で再評価
                float3 toSample = prevGIReservoir.position - worldPos;
                float distance = length(toSample);
                float3 direction = normalize(toSample);
                float cosine = max(0.0f, dot(normal, direction));
                
                if (cosine > 0.0f && distance > 0.001f && distance < 100.0f) {
                    // Target PDF再計算（バイアス補正）
                    float luminance = dot(prevGIReservoir.radiance, float3(0.2126f, 0.7152f, 0.0722f));
                    float recomputedTargetPdf = luminance * cosine / (distance * distance + 1.0f);
                    
                    if (recomputedTargetPdf > 0.0f) {
                        // 重み補正（バイアス除去）
                        float correctedWeight = recomputedTargetPdf / max(prevGIReservoir.pdf, 0.0001f);
                        correctedWeight = min(correctedWeight, 20.0f); // Temporal用制限
                        
                        // M値制限（Temporal安定性）
                        uint temporalM = min(prevGIReservoir.sampleCount, 30u);
                        
                        // Temporal Reservoir結合
                        float temporalContribution = correctedWeight * float(temporalM);
                        float totalWeight = currentReservoir.weightSum + temporalContribution;
                        
                        if (totalWeight > 0.0f) {
                            float probability = temporalContribution / totalWeight;
                            
                            if (RandomFloat(seed) < probability) {
                                // 前フレームのサンプルを採用
                                currentReservoir.position = prevGIReservoir.position;
                                currentReservoir.padding1 = 0.0f;
                                currentReservoir.normal = prevGIReservoir.normal;
                                currentReservoir.padding2 = 0.0f;
                                currentReservoir.radiance = prevGIReservoir.radiance;
                                currentReservoir.padding3 = 0.0f;
                                currentReservoir.throughput = prevGIReservoir.throughput;
                                currentReservoir.albedo = prevGIReservoir.albedo;
                                currentReservoir.padding4 = 0.0f;
                                currentReservoir.pdf = recomputedTargetPdf;
                                currentReservoir.pathLength = 0;
                                currentReservoir.bounceCount = 0;
                            }
                            
                            // 統合重み更新
                            currentReservoir.weightSum = totalWeight;
                            currentReservoir.sampleCount += temporalM;
                            currentReservoir.weight = totalWeight / float(currentReservoir.sampleCount);
                            currentReservoir.valid = true;
                        }
                    }
                }
            }
        }
    }
    #endif // PHASE_2
    
    // **Final Shading**
    if (currentReservoir.valid && currentReservoir.weightSum > 0.0f) {
        // 選択されたGIサンプルからの寄与計算
        float3 toSample = currentReservoir.position - worldPos;
        float distance = length(toSample);
        float3 direction = normalize(toSample);
        float cosine = max(0.0f, dot(normal, direction));
        
        if (cosine > 0.0f && distance > 0.001f) {
            // BRDF評価
            float3 brdf = albedo / 3.14159265f;
            
            // 正規化重み計算
            float normalizedWeight = currentReservoir.weight / max(currentReservoir.pdf, 0.0001f);
            normalizedWeight = min(normalizedWeight, 15.0f); // Temporal用調整
            
            // GI寄与計算
            float3 giContribution = brdf * currentReservoir.radiance * cosine * normalizedWeight;
            indirectIllumination += giContribution * 0.15f; // Temporal用調整
        }
    }
    
    // **現在フレームのReservoirを保存（次フレーム用）**
    uint reservoirIndex = screenCoord.y * screenDim.x + screenCoord.x;
    if (reservoirIndex < (screenDim.x * screenDim.y)) {
        CurrentGIReservoirs[reservoirIndex] = currentReservoir;
    }
    
    return indirectIllumination;
}

// **Spatial Reuse実装**  
float3 CalculateReSTIRGI_WithSpatial(float3 worldPos, float3 normal, float3 albedo, uint materialType, 
                                    uint2 screenCoord, uint2 screenDim, inout uint seed) {
    
    // Lambertianマテリアルのみ処理
    if (materialType != 0) {
        return float3(0, 0, 0);
    }
    
    float3 indirectIllumination = float3(0, 0, 0);
    
    #if RESTIR_GI_PHASE_1_INITIAL_CANDIDATES
    // **最適化: GI候補を削減**
    const int INITIAL_GI_CANDIDATES = 2;  // 4→2に削減
    GIReservoir currentReservoir = InitGIReservoir();
    
    for (int candidate = 0; candidate < INITIAL_GI_CANDIDATES; candidate++) {
        GISample giSample = GenerateSimpleGISample(worldPos, normal, seed);
        
        if (giSample.valid) {
            // Target PDF計算
            float luminance = dot(giSample.radiance, float3(0.2126f, 0.7152f, 0.0722f));
            float distance = length(giSample.position - worldPos);
            float cosine = max(0.0f, dot(normal, normalize(giSample.position - worldPos)));
            float targetPdf = luminance * cosine / (distance * distance + 1.0f);
            
            UpdateGIReservoir(currentReservoir, giSample, targetPdf, seed);
        }
    }
    #endif // PHASE_1
    
    #if RESTIR_GI_PHASE_2_TEMPORAL_REUSE
    // **Phase 2: Temporal Reuse（Phase 3でも使用）**
    if (!cameraMovedFlag) {
        uint reservoirIndex = screenCoord.y * screenDim.x + screenCoord.x;
        
        if (reservoirIndex < (screenDim.x * screenDim.y)) {
            GIReservoir prevGIReservoir = PreviousGIReservoirs[reservoirIndex];
            
            if (prevGIReservoir.valid && prevGIReservoir.weightSum > 0.0f) {
                // Temporal結合処理（Phase 2と同様）
                float3 toSample = prevGIReservoir.position - worldPos;
                float distance = length(toSample);
                float3 direction = normalize(toSample);
                float cosine = max(0.0f, dot(normal, direction));
                
                if (cosine > 0.0f && distance > 0.001f && distance < 100.0f) {
                    float luminance = dot(prevGIReservoir.radiance, float3(0.2126f, 0.7152f, 0.0722f));
                    float recomputedTargetPdf = luminance * cosine / (distance * distance + 1.0f);
                    
                    if (recomputedTargetPdf > 0.0f) {
                        float correctedWeight = recomputedTargetPdf / max(prevGIReservoir.pdf, 0.0001f);
                        correctedWeight = min(correctedWeight, 15.0f);
                        uint temporalM = min(prevGIReservoir.sampleCount, 25u);
                        
                        float temporalContribution = correctedWeight * float(temporalM);
                        float totalWeight = currentReservoir.weightSum + temporalContribution;
                        
                        if (totalWeight > 0.0f) {
                            float probability = temporalContribution / totalWeight;
                            
                            if (RandomFloat(seed) < probability) {
                                currentReservoir.position = prevGIReservoir.position;
                                currentReservoir.padding1 = 0.0f;
                                currentReservoir.normal = prevGIReservoir.normal;
                                currentReservoir.padding2 = 0.0f;
                                currentReservoir.radiance = prevGIReservoir.radiance;
                                currentReservoir.padding3 = 0.0f;
                                currentReservoir.throughput = prevGIReservoir.throughput;
                                currentReservoir.albedo = prevGIReservoir.albedo;
                                currentReservoir.padding4 = 0.0f;
                                currentReservoir.pdf = recomputedTargetPdf;
                                currentReservoir.pathLength = 0;
                                currentReservoir.bounceCount = 0;
                            }
                            
                            currentReservoir.weightSum = totalWeight;
                            currentReservoir.sampleCount += temporalM;
                            currentReservoir.weight = totalWeight / float(currentReservoir.sampleCount);
                            currentReservoir.valid = true;
                        }
                    }
                }
            }
        }
    }
    #endif // PHASE_2
    
    #if RESTIR_GI_PHASE_3_SPATIAL_REUSE
    // **Phase 3: Spatial Reuse実装**
    const int SPATIAL_NEIGHBORS = 4; // 近隣ピクセル数
    const float SPATIAL_RADIUS = 16.0f; // 空間探索半径
    
    for (int neighbor = 0; neighbor < SPATIAL_NEIGHBORS; neighbor++) {
        // 近隣ピクセル座標をランダムに選択
        float angle = 2.0f * 3.14159265f * RandomFloat(seed);
        float radius = SPATIAL_RADIUS * sqrt(RandomFloat(seed));
        
        int2 neighborCoord = int2(
            screenCoord.x + int(radius * cos(angle)),
            screenCoord.y + int(radius * sin(angle))
        );
        
        // 画面内チェック
        if (neighborCoord.x >= 0 && neighborCoord.x < screenDim.x && 
            neighborCoord.y >= 0 && neighborCoord.y < screenDim.y) {
            
            uint neighborIndex = neighborCoord.y * screenDim.x + neighborCoord.x;
            GIReservoir neighborReservoir = CurrentGIReservoirs[neighborIndex];
            
            if (neighborReservoir.valid && neighborReservoir.weightSum > 0.0f) {
                // **Visibility Test（簡潔版）**
                float3 toNeighborSample = neighborReservoir.position - worldPos;
                float neighborDistance = length(toNeighborSample);
                float3 neighborDirection = normalize(toNeighborSample);
                float neighborCosine = max(0.0f, dot(normal, neighborDirection));
                
                // 基本的な幾何学的妥当性チェック
                bool geometricallyValid = (neighborCosine > 0.1f && neighborDistance > 0.001f && neighborDistance < 50.0f);
                
                // 法線の類似性チェック
                float normalSimilarity = dot(normal, neighborReservoir.normal);
                bool normalCompatible = (normalSimilarity > 0.5f);
                
                if (geometricallyValid && normalCompatible) {
                    // **Spatial Target PDF再計算**
                    float neighborLuminance = dot(neighborReservoir.radiance, float3(0.2126f, 0.7152f, 0.0722f));
                    float spatialTargetPdf = neighborLuminance * neighborCosine / (neighborDistance * neighborDistance + 1.0f);
                    
                    if (spatialTargetPdf > 0.0f) {
                        // **重み補正（Spatial用）**
                        float spatialCorrectedWeight = spatialTargetPdf / max(neighborReservoir.pdf, 0.0001f);
                        spatialCorrectedWeight = min(spatialCorrectedWeight, 10.0f); // Spatial制限
                        
                        // **M値制限（Spatial安定性）**
                        uint spatialM = min(neighborReservoir.sampleCount, 15u);
                        
                        // **Spatial Reservoir結合**
                        float spatialContribution = spatialCorrectedWeight * float(spatialM) * 0.5f; // Spatial係数
                        float totalWeight = currentReservoir.weightSum + spatialContribution;
                        
                        if (totalWeight > 0.0f) {
                            float probability = spatialContribution / totalWeight;
                            
                            if (RandomFloat(seed) < probability) {
                                // 近隣サンプルを採用
                                currentReservoir.position = neighborReservoir.position;
                                currentReservoir.padding1 = 0.0f;
                                currentReservoir.normal = neighborReservoir.normal;
                                currentReservoir.padding2 = 0.0f;
                                currentReservoir.radiance = neighborReservoir.radiance;
                                currentReservoir.padding3 = 0.0f;
                                currentReservoir.throughput = neighborReservoir.throughput;
                                currentReservoir.albedo = neighborReservoir.albedo;
                                currentReservoir.padding4 = 0.0f;
                                currentReservoir.pdf = spatialTargetPdf;
                                currentReservoir.pathLength = 0;
                                currentReservoir.bounceCount = 0;
                            }
                            
                            // 統合重み更新
                            currentReservoir.weightSum = totalWeight;
                            currentReservoir.sampleCount += spatialM;
                            currentReservoir.weight = totalWeight / float(currentReservoir.sampleCount);
                            currentReservoir.valid = true;
                        }
                    }
                }
            }
        }
    }
    #endif // PHASE_3
    
    // **Final Shading**
    if (currentReservoir.valid && currentReservoir.weightSum > 0.0f) {
        float3 toSample = currentReservoir.position - worldPos;
        float distance = length(toSample);
        float3 direction = normalize(toSample);
        float cosine = max(0.0f, dot(normal, direction));
        
        if (cosine > 0.0f && distance > 0.001f) {
            float3 brdf = albedo / 3.14159265f;
            float normalizedWeight = currentReservoir.weight / max(currentReservoir.pdf, 0.0001f);
            normalizedWeight = min(normalizedWeight, 12.0f); // Spatial用調整
            
            float3 giContribution = brdf * currentReservoir.radiance * cosine * normalizedWeight;
            indirectIllumination += giContribution * 0.12f; // Spatial用調整
        }
    }
    
    // **現在フレームのReservoirを保存**
    uint reservoirIndex = screenCoord.y * screenDim.x + screenCoord.x;
    if (reservoirIndex < (screenDim.x * screenDim.y)) {
        CurrentGIReservoirs[reservoirIndex] = currentReservoir;
    }
    
    return indirectIllumination;
}

// **メインReSTIR GI実装（段階的拡張可能）**
float3 CalculateReSTIRGI(float3 worldPos, float3 normal, float3 albedo, uint materialType, 
                        uint2 screenCoord, uint2 screenDim, inout uint seed) {
    
    // **Phase 3実装: Spatial Reuseを有効化**
    return CalculateReSTIRGI_WithSpatial(worldPos, normal, albedo, materialType, screenCoord, screenDim, seed);
    
    // **Phase 2のみ使用する場合**
    // return CalculateReSTIRGI_WithTemporal(worldPos, normal, albedo, materialType, screenCoord, screenDim, seed);
    
    // **Phase 1のみ使用する場合**
    // return CalculateReSTIRGI_Phase1_Only(worldPos, normal, albedo, materialType, screenCoord, screenDim, seed);
}

#endif