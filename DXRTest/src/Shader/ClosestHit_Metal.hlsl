#include "Common.hlsli"

BRDFSample SampleMetalBRDF(float3 normal, float3 rayDir, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    float3 reflected = reflect(rayDir, normal);
    
    float3 perturbation = material.roughness * RandomUnitVector(seed);
    sample.direction = normalize(reflected + perturbation);
    
    float NdotL = dot(sample.direction, normal);
    if (NdotL > 0.0f)
    {
        sample.brdf = material.albedo;
        sample.pdf = 1.0f;
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
    }
    
    return sample;
}

float3 CalculateDirectLighting(float3 worldPos, float3 normal, MaterialData material, inout uint seed)
{
    float3 directLighting = 0.0f;
    
    if (numLights == 0)
        return directLighting;
    
    uint maxLightsToSample = min(numLights, 3u);
    
    for (uint lightIdx = 0; lightIdx < maxLightsToSample; lightIdx++)
    {
        LightSample lightSample = SampleLightByIndex(lightIdx, worldPos, seed);
        
        if (lightSample.valid)
        {
            float NdotL = max(0.0f, dot(normal, lightSample.direction));
            
            if (NdotL > 0.0f)
            {
                RayDesc shadowRay;
                shadowRay.Origin = OffsetRay(worldPos, normal);
                shadowRay.Direction = lightSample.direction;
                shadowRay.TMin = 0.001f;
                shadowRay.TMax = lightSample.distance - 0.001f;
                
                RayPayload shadowPayload;
                shadowPayload.color = float3(1, 1, 1);
                shadowPayload.depth = 999;
                shadowPayload.seed = seed;
                
                TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                         0xFF, 0, 1, 0, shadowRay, shadowPayload);
                
                if (length(shadowPayload.color) > 0.5f)
                {
                    float3 brdf;
                    if (material.roughness > 0.05f)
                    {
                        brdf = material.albedo * (0.8f + 0.2f * material.roughness);
                    }
                    else
                    {
                        brdf = material.albedo;
                    }
                    
                    float3 directContribution = brdf * lightSample.radiance * NdotL;
                    
                    if (material.roughness > 0.1f)
                    {
                        float3 lightDir = lightSample.direction;
                        float3 viewDir = -normalize(WorldRayDirection());
                        float3 halfVector = normalize(lightDir + viewDir);
                        float brdfPdf = max(0.0f, dot(halfVector, normal)) / PI;
                        
                        float misWeight = MISWeightLight(lightSample.pdf, brdfPdf);
                        directLighting += directContribution * misWeight / lightSample.pdf;
                    }
                    else
                    {
                        directLighting += directContribution / lightSample.pdf;
                    }
                }
            }
        }
    }
    
    if (maxLightsToSample > 1)
    {
        directLighting /= float(maxLightsToSample);
    }
    
    return directLighting;
}

float3 CalculateIndirectLighting(float3 worldPos, float3 normal, MaterialData material,
                                float3 rayDir, uint depth, inout uint seed)
{
    float3 indirectLighting = 0.0f;
    
    if (depth >= 5)
        return indirectLighting;
    
    BRDFSample brdfSample = SampleMetalBRDF(normal, rayDir, material, seed);
    
    if (brdfSample.valid)
    {
        RayDesc ray;
        ray.Origin = OffsetRay(worldPos, normal);
        ray.Direction = brdfSample.direction;
        ray.TMin = 0.001f;
        ray.TMax = 1000.0f;
        
        RayPayload newPayload;
        newPayload.color = float3(0, 0, 0);
        newPayload.depth = depth + 1;
        newPayload.seed = seed;
        
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
            float NdotL = max(0.0f, dot(normal, brdfSample.direction));
            float3 reflectionContribution = brdfSample.brdf * newPayload.color * NdotL;
            
            indirectLighting += reflectionContribution / brdfSample.pdf;
        }
    }
    
    return indirectLighting;
}

[shader("closesthit")]
void ClosestHit_Metal(inout RayPayload payload, in VertexAttributes attr)
{
    bool metalDebugMode = false;
    if (metalDebugMode && payload.depth == 0)
    {
        payload.color = float3(0.0f, 0.0f, 1.0f);
        return;
    }
    
    if (payload.depth >= 3)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    float3 rayDir = normalize(WorldRayDirection());
    if (dot(normal, rayDir) > 0.0f)
    {
        normal = -normal;
    }
    
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_METAL, material.roughness, RayTCurrent());
    
    
    float3 directLighting = CalculateDirectLighting(worldPos, normal, material, payload.seed);
    
    float3 indirectLighting = CalculateIndirectLighting(worldPos, normal, material,
                                                        rayDir, payload.depth, payload.seed);
    
    payload.color = indirectLighting;
}