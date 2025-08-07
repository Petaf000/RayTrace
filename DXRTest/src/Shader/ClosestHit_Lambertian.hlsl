// ===== ClosestHit_Lambertian.hlsl NEE+MIS完全実装版（直接光・間接光分離） =====
#include "Common.hlsli"

// **直接照明計算関数（NEE + MIS）**
float3 ComputeDirectLighting(float3 worldPos, float3 normal, MaterialData material, 
                           uint depth, inout uint seed)
{
    float3 directColor = 0.0f;
    bool useMIS = (depth == 0); // プライマリレイでのみMIS使用
    
    if (numLights > 0) {
        uint maxLightsToSample = min(numLights, 4u);
        
        for (uint lightIdx = 0; lightIdx < maxLightsToSample; lightIdx++) {
            // 適応的ソフトシャドウサンプリング（品質重視で調査）
            const uint strataX = (depth == 0) ? 3u : 1u;
            const uint strataY = (depth == 0) ? 3u : 1u;
            const uint totalStrata = strataX * strataY;
            
            float3 totalContribution = 0.0f;
            
            // 各格子セルから1つずつサンプリング
            for (uint sy = 0; sy < strataY; sy++) {
                for (uint sx = 0; sx < strataX; sx++) {
                    uint stratumSeed = seed + sy * strataX + sx + lightIdx * 12347;
                    
                    LightSample lightSample = SampleLightByIndexStratified(
                        lightIdx, worldPos, stratumSeed, sx, sy, strataX, strataY);
                    
                    if (lightSample.valid) {
                        float NdotL = max(0.0f, dot(normal, lightSample.direction));
                        
                        if (NdotL > 0.0f) {
                            // シャドウレイで可視性チェック
                            RayDesc shadowRay;
                            float offsetDistance = 0.005f;
                            float3 offsetPos = worldPos + normal * offsetDistance;
                            float3 lightDir = normalize(lightSample.direction);
                            offsetPos += lightDir * 0.001f;
                            
                            shadowRay.Origin = offsetPos;
                            shadowRay.Direction = lightSample.direction;
                            shadowRay.TMin = 0.002f;
                            shadowRay.TMax = max(0.01f, lightSample.distance - 0.01f);
                            
                            RayPayload shadowPayload;
                            shadowPayload.color = float3(1, 1, 1);
                            shadowPayload.depth = 999;
                            shadowPayload.seed = stratumSeed;
                            
                            TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                                     0xFF, 0, 1, 0, shadowRay, shadowPayload);
                            
                            if (length(shadowPayload.color) > 0.5f) {
                                // Lambert BRDF値
                                float3 brdf = material.albedo / PI;
                                float3 directContribution = brdf * lightSample.radiance * NdotL;
                                
                                if (useMIS) {
                                    // MIS適用
                                    float cosTheta = max(0.0f, dot(lightSample.direction, normal));
                                    float brdfPdf = cosTheta / PI;
                                    float misWeight = MISWeightLight(lightSample.pdf, brdfPdf);
                                    
                                    float3 contribution = directContribution * misWeight / lightSample.pdf;
                                    totalContribution += contribution;
                                } else {
                                    float3 contribution = directContribution / lightSample.pdf;
                                    totalContribution += contribution;
                                }
                            }
                        }
                    }
                }
            }
            
            // 格子全体の平均を取る
            directColor += totalContribution / float(totalStrata);
        }
    }
    
    return directColor;
}

// **間接照明計算関数（Monte Carlo）**
float3 ComputeIndirectLighting(float3 worldPos, float3 normal, MaterialData material, 
                             uint depth, inout uint seed)
{
    float3 indirectColor = 0.0f;
    
    // 深いレイでは間接照明を省略（パフォーマンス重視）
    if (depth >= 2) {
        return indirectColor;
    }
    
    // **安全なサンプル数（ハングアップ防止）**
    const uint indirectSamples = 1; // 1サンプルで安全性確保
    float3 totalIndirectContribution = 0.0f;
    
    for (uint i = 0; i < indirectSamples; i++) {
        // 時間的蓄積対応サンプリング（正しいMonte Carlo積分）
        uint frameOffset = uint(frameCount) % 8u; // 8パターンでサイクル
        uint indirectSeed = seed + (i * 15485863u) + (frameOffset * 982451653u);
        
        // 方向ベクトル生成（既存の安全な実装を使用）
        float3 indirectDirection;
        bool validDirection = false;
        int attempts = 0;
        
        while (!validDirection && attempts < 5) {
            uint currentSeed = indirectSeed + attempts * 31337u;
            
            if (attempts == 0) {
                // 方法1: RandomCosineDirection
                float3 localDir = RandomCosineDirection(currentSeed);
                float3 w = normal;
                float3 u = normalize(cross((abs(w.x) > 0.1f ? float3(0, 1, 0) : float3(1, 0, 0)), w));
                float3 v = cross(w, u);
                indirectDirection = localDir.x * u + localDir.y * v + localDir.z * w;
            } else if (attempts == 1) {
                // 方法2: 球面座標
                float r1 = RandomFloat(currentSeed);
                float r2 = RandomFloat(currentSeed);
                float cosTheta = sqrt(1.0f - r2);
                float sinTheta = sqrt(r2);
                float phi = 2.0f * PI * r1;
                float3 localDir = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
                float3 nt = (abs(normal.x) > abs(normal.y)) ? 
                            float3(normal.z, 0, -normal.x) / sqrt(normal.x * normal.x + normal.z * normal.z) :
                            float3(0, -normal.z, normal.y) / sqrt(normal.y * normal.y + normal.z * normal.z);
                float3 nb = cross(normal, nt);
                indirectDirection = localDir.x * nb + localDir.y * nt + localDir.z * normal;
            } else {
                // 方法3: フォールバック
                float3 randomDir = RandomUnitVector(currentSeed);
                if (dot(randomDir, normal) < 0.0f) {
                    randomDir = -randomDir;
                }
                indirectDirection = randomDir;
            }
            
            indirectDirection = normalize(indirectDirection);
            
            // 妥当性チェック
            if (!any(isnan(indirectDirection)) && !any(isinf(indirectDirection))) {
                float dirLength = length(indirectDirection);
                float dotWithNormal = dot(indirectDirection, normal);
                if (dirLength > 0.9f && dirLength < 1.1f && dotWithNormal > 0.0f) {
                    validDirection = true;
                }
            }
            attempts++;
        }
        
        if (!validDirection) {
            indirectDirection = normal; // 安全なフォールバック
        }
        
        // BRDF値とPDF計算
        float3 brdf = material.albedo / PI;
        float cosTheta = max(0.0f, dot(indirectDirection, normal));
        float pdf = cosTheta / PI;
        
        if (pdf > 0.0001f) {
            // 間接照明レイトレース
            RayDesc ray;
            ray.Origin = OffsetRay(worldPos, normal);
            ray.Direction = indirectDirection;
            ray.TMin = 0.001f;
            ray.TMax = 1000.0f;
            
            RayPayload newPayload;
            newPayload.color = float3(0, 0, 0);
            newPayload.depth = depth + 1;
            newPayload.seed = indirectSeed;
            
            // G-Bufferデータ初期化
            newPayload.albedo = float3(0, 0, 0);
            newPayload.normal = float3(0, 0, 1);
            newPayload.worldPos = float3(0, 0, 0);
            newPayload.hitDistance = 0.0f;
            newPayload.materialType = 0;
            newPayload.roughness = 0.0f;
            newPayload.padding = 0;
            
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, newPayload);
            
            // 間接照明の寄与計算
            if (length(newPayload.color) > 0.0f) {
                float3 secondaryColor = newPayload.color;
                
                // 異常値チェック
                if (any(isnan(secondaryColor)) || any(isinf(secondaryColor))) {
                    secondaryColor = float3(0, 0, 0);
                }
                
                secondaryColor = clamp(secondaryColor, 0.0f, 50.0f);
                
                // Monte Carlo積分
                float3 indirectContribution = brdf * secondaryColor * cosTheta;
                float3 sampleContribution = indirectContribution / pdf;
                
                // 分散削減
                float contributionMagnitude = length(sampleContribution);
                if (contributionMagnitude > 0.0f) {
                    float maxContribution = 2.0f;
                    if (contributionMagnitude > maxContribution) {
                        sampleContribution *= maxContribution / contributionMagnitude;
                    }
                }
                
                // 異常値チェック
                if (any(isnan(sampleContribution)) || any(isinf(sampleContribution))) {
                    sampleContribution = float3(0, 0, 0);
                }
                
                sampleContribution = clamp(sampleContribution, 0.0f, 10.0f);
                totalIndirectContribution += sampleContribution;
            }
        }
    }
    
    // 複数サンプルの平均
    indirectColor = totalIndirectContribution / float(indirectSamples);
    
    return indirectColor;
}

// LAMBERTIAN拡散BRDF サンプリング
BRDFSample SampleLambertianBRDF(float3 normal, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    // コサイン重み付きサンプリング
    float r1 = RandomFloat(seed);
    float r2 = RandomFloat(seed);
    
    float cosTheta = sqrt(1.0f - r2);
    float sinTheta = sqrt(r2);
    float phi = 2.0f * PI * r1;
    
    // 接線座標系での方向
    float3 localDir = float3(
        sinTheta * cos(phi),
        sinTheta * sin(phi),
        cosTheta
    );
    
    // 法線軸の座標系に変換
    float3 up = abs(normal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    sample.direction = localDir.x * tangent + localDir.y * bitangent + localDir.z * normal;
    sample.brdf = material.albedo / PI; // Lambertian BRDF
    sample.pdf = cosTheta / PI; // 射影立体角PDF（cos重み付き）
    sample.valid = true;
    
    return sample;
}

[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    // **G-Bufferモード：マテリアル情報のみ提供（ReSTIR統合型アーキテクチャ）**
    int debugMode = 3; // 3=G-Bufferモード（照明計算なし、マテリアル情報のみ）
    
    // 最大反射回数チェック
    if (payload.depth >= 3)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    uint instanceID = InstanceID();
    uint primitiveID = PrimitiveIndex();
    MaterialData material = GetMaterial(instanceID);
    
    // 衝突点計算
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // 正確な法線をワールド空間で取得
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    // レイ方向と法線の向き確認
    float3 rayDir = normalize(WorldRayDirection());
    if (dot(normal, rayDir) > 0.0f)
    {
        normal = -normal;
    }
    
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_LAMBERTIAN, material.roughness, RayTCurrent());
    
    // **物理的スケール用AO計算**
    float aoFactor = 1.0f;
    if (payload.depth == 0) { // プライマリレイでのみAO計算
        float occluded = 0.0f;
        const int aoSamples = 6; // AO専用サンプル数（品質重視で調査）
        const float aoRadius = 0.3f; // AOの影響半径（30cm - より広範囲）
        
        for (int i = 0; i < aoSamples; i++) {
            // 各サンプルで異なるシードを使用
            uint sampleSeed = payload.seed + i * 7919; // 素数でシード分散
            
            BRDFSample aoSample = SampleLambertianBRDF(normal, material, sampleSeed);
            
            if (aoSample.valid) {
                // AO専用の短距離レイ
                RayDesc aoRay;
                aoRay.Origin = OffsetRay(worldPos, normal);
                aoRay.Direction = aoSample.direction;
                aoRay.TMin = 0.01f;
                aoRay.TMax = aoRadius; // 短距離でのオクルージョン検出
                
                // AO専用のペイロード
                RayPayload aoPayload;
                aoPayload.color = float3(1, 1, 1); // ヒットしなければ1
                aoPayload.depth = 999; // 深い値でAOレイと識別
                aoPayload.seed = sampleSeed;
                
                // G-Buffer初期化
                aoPayload.albedo = float3(0, 0, 0);
                aoPayload.normal = float3(0, 0, 1);
                aoPayload.worldPos = float3(0, 0, 0);
                aoPayload.hitDistance = 0.0f;
                aoPayload.materialType = 0;
                aoPayload.roughness = 0.0f;
                aoPayload.padding = 0;
                
                // AOレイトレーシング実行
                TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 
                         0xFF, 0, 1, 0, aoRay, aoPayload);
                
                // ヒットした場合（遮蔽された場合）
                if (length(aoPayload.color) < 0.5f) {
                    // 距離による重み付け：近いほど強い遮蔽効果
                    float distance = aoPayload.hitDistance;
                    float weight = 1.0f - (distance / aoRadius);
                    weight = max(0.0f, weight);
                    
                    // コサイン重み付け
                    float cosTheta = max(0.0f, dot(normal, aoSample.direction));
                    occluded += weight * cosTheta;
                }
            }
        }
        
        // AO係数計算（0=完全遮蔽, 1=遮蔽なし）
        // normalizerは理論的な最大遮蔽値（全サンプルが完全遮蔽された場合）
        float normalizer = float(aoSamples); // 最大遮蔽値
        
        if (normalizer > 0.0f) {
            aoFactor = 1.0f - (occluded / normalizer);
            aoFactor = max(0.1f, aoFactor); // 完全黒を避ける
        }
    }
    
    // **デバッグモード3: AO係数表示**
    if (debugMode == 3 && payload.depth == 0) {
        float aoGray = aoFactor;
        payload.color = float3(aoGray, aoGray, aoGray);
        return;
    }
    
    // **分離されたライティング計算（ReSTIR準備）**
    float3 directColor = ComputeDirectLighting(worldPos, normal, material, payload.depth, payload.seed);
    float3 indirectColor = ComputeIndirectLighting(worldPos, normal, material, payload.depth, payload.seed);
    
    // 最終色の合成
    float3 finalColor = directColor + indirectColor;
    
    // **最終色の異常値チェック**
    if (any(isnan(finalColor)) || any(isinf(finalColor))) {
        finalColor = float3(0, 0, 0); // 異常値は黒に設定
    }
    
    // **最終色の範囲制限**
    finalColor = clamp(finalColor, 0.0f, 1000.0f);
    
    // **デバッグモード1: 直接照明のみ**
    if (debugMode == 1 && payload.depth == 0) {
        payload.color = directColor * aoFactor;
        return;
    }
    
    // **デバッグモード2: 間接照明のみ**
    if (debugMode == 2 && payload.depth == 0) {
        payload.color = indirectColor * aoFactor;
        return;
    }
    
    // **G-Bufferモード: マテリアル情報のみ（照明計算なし）**
    if (debugMode == 3) {
        // G-Bufferデータのみ設定、照明計算は全てRayGenで実行
        payload.color = float3(0, 0, 0); // 照明なし
        // worldPos, normal, albedo, materialTypeなどは既に設定済み
        return;
    }
    
    // AO係数を適用した最終色計算
    payload.color = finalColor * aoFactor;
}