#include "../Common.hlsli"

float3 ComputeDirectLighting(float3 worldPos, float3 normal, MaterialData material,
                           uint depth, inout uint seed)
{
    float3 directColor = 0.0f;
    uint maxLights = min(numLights, 4u);
    
    const uint samples = (depth == 0) ? 4u : 1u;
    
    for (uint lightIdx = 0; lightIdx < maxLights; lightIdx++)
    {
        float3 lightContribution = 0.0f;
        for (uint s = 0; s < samples; s++)
        {
            uint lightSeed = seed + s + lightIdx * 12347;
            LightSample lightSample = SampleLightByIndex(lightIdx, worldPos, lightSeed);
            
            if (lightSample.valid)
            {
                float NdotL = max(0.0f, dot(normal, lightSample.direction));
                if (NdotL > 0.0f)
                {
                    if (TestLightVisibility(worldPos, normal, lightSample.direction, lightSample.distance, lightSeed))
                    {
                        float3 brdf = material.albedo / PI;
                        float3 contribution = brdf * lightSample.radiance * NdotL / lightSample.pdf;
                        
                        if (depth == 0)
                        {
                            float brdfPdf = NdotL / PI;
                            float misWeight = MISWeightLight(lightSample.pdf, brdfPdf);
                            contribution *= misWeight;
                        }
                        
                        lightContribution += contribution;
                    }
                }
            }
        }
        
        directColor += lightContribution / float(samples);
    }
    
    return directColor;
}

float3 ComputeIndirectLighting(float3 worldPos, float3 normal, MaterialData material,
                             uint depth, inout uint seed)
{
    if (depth >= 2)
    {
        return float3(0, 0, 0);
    }
    
    uint frameOffset = uint(frameCount) % 8u;
    uint indirectSeed = seed + (frameOffset * 982451653u);
    
    // コサイン重み付きサンプリング
    float3 localDir = RandomCosineDirection(indirectSeed);
    float3 w = normal;
    float3 u = normalize(cross((abs(w.x) > 0.1f ? float3(0, 1, 0) : float3(1, 0, 0)), w));
    float3 v = cross(w, u);
    float3 indirectDirection = localDir.x * u + localDir.y * v + localDir.z * w;
    
    float3 brdf = material.albedo / PI;
    float cosTheta = max(0.0f, dot(indirectDirection, normal));
    float pdf = cosTheta / PI;
    
    if (pdf > 0.0001f)
    {
        RayDesc ray;
        ray.Origin = OffsetRay(worldPos, normal);
        ray.Direction = indirectDirection;
        ray.TMin = 0.001f;
        ray.TMax = 1000.0f;
        
        RayPayload newPayload;
        newPayload.color = float3(0, 0, 0);
        newPayload.depth = depth + 1;
        newPayload.seed = indirectSeed;
        
        newPayload.albedo = float3(0, 0, 0);
        newPayload.normal = float3(0, 0, 1);
        newPayload.worldPos = float3(0, 0, 0);
        newPayload.hitDistance = 0.0f;
        newPayload.materialType = 0;
        newPayload.roughness = 0.0f;
        newPayload.padding = 0;
        
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, newPayload);
        
        if (length(newPayload.color) > 0.0f)
        {
            float3 secondaryColor = clamp(newPayload.color, 0.0f, 50.0f);
            float3 contribution = brdf * secondaryColor * cosTheta / pdf;
            
            if (!any(isnan(contribution)) && !any(isinf(contribution)))
            {
                float maxContribution = 2.0f;
                float magnitude = length(contribution);
                if (magnitude > maxContribution)
                {
                    contribution *= maxContribution / magnitude;
                }
                
                return clamp(contribution, 0.0f, 10.0f);
            }
        }
    }
    
    return float3(0, 0, 0);
}

float3 ComputeAmbientOcclusion(float3 worldPos, float3 normal, MaterialData material, inout uint seed)
{
    float occluded = 0.0f;
    // リファレンスにできるだけ近づけるために目で調整してます
    const int aoSamples = 16;
    const float aoRadius = 0.05f;
    
    for (int i = 0; i < aoSamples; i++)
    {
        uint sampleSeed = seed + i * 7919;
        // コサインサンプリング
        float3 localDir = RandomCosineDirection(sampleSeed);
        float3 w = normal;
        float3 u = normalize(cross((abs(w.x) > 0.1f ? float3(0, 1, 0) : float3(1, 0, 0)), w));
        float3 v = cross(w, u);
        float3 aoDirection = localDir.x * u + localDir.y * v + localDir.z * w;
        
        
        RayDesc aoRay;
        aoRay.Origin = OffsetRay(worldPos, normal);
        aoRay.Direction = aoDirection;
        aoRay.TMin = 0.01f;
        aoRay.TMax = aoRadius;
        
        RayPayload aoPayload;
        aoPayload.color = float3(1, 1, 1);
        aoPayload.depth = 999;
        aoPayload.seed = sampleSeed;
        
        // G-Bufferデータの初期化
        aoPayload.albedo = float3(0, 0, 0);
        aoPayload.normal = float3(0, 0, 1);
        aoPayload.worldPos = float3(0, 0, 0);
        aoPayload.hitDistance = 0.0f;
        aoPayload.materialType = 0;
        aoPayload.roughness = 0.0f;
        aoPayload.padding = 0;
        
        TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                 0xFF, 0, 1, 0, aoRay, aoPayload);
        
        if (length(aoPayload.color) < 0.5f)
        {
            float distance = aoPayload.hitDistance;
            float weight = 1.0f - (distance / aoRadius);
            float cosTheta = max(0.0f, dot(normal, aoDirection));
            occluded += max(0.0f, weight) * cosTheta;
        }
    }
    
    float aoFactor = 1.0f - (occluded / float(aoSamples));
    return float3(max(0.1f, aoFactor), max(0.1f, aoFactor), max(0.1f, aoFactor));
}

[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    if (payload.depth >= 3)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    uint instanceID = InstanceID();
    uint primitiveID = PrimitiveIndex();
    MaterialData material = GetMaterial(instanceID);
    
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    // 法線の向きを修正
    float3 rayDir = normalize(WorldRayDirection());
    if (dot(normal, rayDir) > 0.0f)
    {
        normal = -normal;
    }
    
    // G-Bufferデータの設定
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_LAMBERTIAN, material.roughness, RayTCurrent());
                   
    // ライティング計算
    float3 directColor = ComputeDirectLighting(worldPos, normal, material, payload.depth, payload.seed);
    float3 indirectColor = ComputeIndirectLighting(worldPos, normal, material, payload.depth, payload.seed);
    
    // アンビエントオクルージョン
    float3 aoFactor = float3(1, 1, 1);
    if (payload.depth == 0)
    {
        aoFactor = ComputeAmbientOcclusion(worldPos, normal, material, payload.seed);
    }
    
    float3 finalColor = directColor + indirectColor;
    if (any(isnan(finalColor)) || any(isinf(finalColor)))
    {
        finalColor = float3(0, 0, 0);
    }
    
    finalColor = clamp(finalColor, 0.0f, 1000.0f);
    payload.color = finalColor * aoFactor;
}