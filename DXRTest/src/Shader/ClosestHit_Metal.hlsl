// ===== ClosestHit_Metal.hlsl NEE+MIS対応版 =====
#include "Common.hlsli"

// Metal反射用BRDF サンプリング
BRDFSample SampleMetalBRDF(float3 normal, float3 rayDir, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    // 鏡面反射方向
    float3 reflected = reflect(rayDir, normal);
    
    // ラフネスによるランダム摂動追加
    float3 perturbation = material.roughness * RandomUnitVector(seed);
    sample.direction = normalize(reflected + perturbation);
    
    // 法線との角度チェック
    float NdotL = dot(sample.direction, normal);
    if (NdotL > 0.0f)
    {
        sample.brdf = material.albedo; // 簡易BRDF
        sample.pdf = 1.0f; // デルタ関数的（スペキュラー）
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
    }
    
    return sample;
}

[shader("closesthit")]
void ClosestHit_Metal(inout RayPayload payload, in VertexAttributes attr)
{
    // デバッグモード：Metalシェーダー動作確認
    bool metalDebugMode = false;
    if (metalDebugMode && payload.depth == 0) {
        payload.color = float3(0.0f, 0.0f, 1.0f); // 青色：Metalシェーダー実行
        return;
    }
    // 最大反射回数チェック
    if (payload.depth >= 3)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    // セカンダリレイでも通常処理を行う（簡略化を除去）
    
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    // 衝突点計算
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // 正確な法線を頂点データから取得
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    // レイ方向と法線の向き確認
    float3 rayDir = normalize(WorldRayDirection());
    if (dot(normal, rayDir) > 0.0f)
    {
        normal = -normal;
    }
    
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_METAL, material.roughness, RayTCurrent());
    
    // Metal材質でのレンダリング（NEE+MIS対応）
    float3 finalColor = 0.0f;
    
    // Metalマテリアルは主に反射だが、ラフネスが高い場合はNEEも有効
    bool useDirectLighting = (material.roughness > 0.1f);
    bool useMIS = (payload.depth > 0 && useDirectLighting);
    
    // 1. Next Event Estimation（ラフネスがある場合のみ）
    if (useDirectLighting && numLights > 0) {
        uint maxLightsToSample = min(numLights, 2u); // Metalは反射主体なのでライト数制限
        
        for (uint lightIdx = 0; lightIdx < maxLightsToSample; lightIdx++) {
            LightSample lightSample = SampleLightByIndex(lightIdx, worldPos, payload.seed);
            
            if (lightSample.valid) {
                float NdotL = max(0.0f, dot(normal, lightSample.direction));
                
                if (NdotL > 0.0f) {
                    // シャドウレイで可視性チェック
                    RayDesc shadowRay;
                    shadowRay.Origin = OffsetRay(worldPos, normal);
                    shadowRay.Direction = lightSample.direction;
                    shadowRay.TMin = 0.001f;
                    shadowRay.TMax = lightSample.distance - 0.001f;
                    
                    RayPayload shadowPayload;
                    shadowPayload.color = float3(1, 1, 1);
                    shadowPayload.depth = 999;
                    shadowPayload.seed = payload.seed;
                    
                    TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                             0xFF, 0, 1, 0, shadowRay, shadowPayload);
                    
                    if (length(shadowPayload.color) > 0.5f) {
                        // Metal BRDF（簡易版：フレネル項は省略）
                        float3 brdf = material.albedo * material.roughness; // ラフネスで調整
                        float3 directContribution = brdf * lightSample.radiance * NdotL;
                        
                        if (useMIS) {
                            // Metal用のBRDF PDF（簡易版）
                            float3 reflected = reflect(-lightSample.direction, normal);
                            float brdfPdf = max(0.0f, dot(reflected, normal)) / PI;
                            
                            float misWeight = MISWeightLight(lightSample.pdf, brdfPdf);
                            finalColor += directContribution * misWeight / lightSample.pdf;
                        } else {
                            finalColor += directContribution / lightSample.pdf;
                        }
                    }
                }
            }
        }
        
        if (maxLightsToSample > 1) {
            finalColor /= float(maxLightsToSample);
        }
    }
    
    // 2. 反射レイトレース（Metal材質の主要コンポーネント）
    if (payload.depth < 5) {
        BRDFSample brdfSample = SampleMetalBRDF(normal, rayDir, material, payload.seed);
        
        if (brdfSample.valid) {
            // 反射レイトレース
            RayDesc ray;
            ray.Origin = OffsetRay(worldPos, normal);
            ray.Direction = brdfSample.direction;
            ray.TMin = 0.001f;
            ray.TMax = 1000.0f;
            
            RayPayload newPayload;
            newPayload.color = float3(0, 0, 0);
            newPayload.depth = payload.depth + 1;
            newPayload.seed = payload.seed;
            
            // G-Bufferデータ初期化
            newPayload.albedo = float3(0, 0, 0);
            newPayload.normal = float3(0, 0, 1);
            newPayload.worldPos = float3(0, 0, 0);
            newPayload.hitDistance = 0.0f;
            newPayload.materialType = 0;
            newPayload.roughness = 0.0f;
            newPayload.padding = 0;
            
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, newPayload);
            
            // 反射の寄与
            if (length(newPayload.color) > 0.0f) {
                float3 reflectionContribution = brdfSample.brdf * newPayload.color * max(0.0f, dot(normal, brdfSample.direction));
                
                // Metalの反射はほぼ完全なのでMIS重み1.0を使用
                finalColor += reflectionContribution / brdfSample.pdf;
            }
        }
    }
    
    payload.color = finalColor;
}