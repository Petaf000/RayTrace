#include "Common.hlsli"

float schlick(float cosine, float ri)
{
    float r0 = (1.0f - ri) / (1.0f + ri);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

float3 reflect_vec(float3 v, float3 n)
{
    return v - 2.0f * dot(v, n) * n;
}

bool refract_vec(float3 v, float3 n, float ni_over_nt, out float3 refracted)
{
    float3 uv = normalize(v);
    float dt = dot(uv, n);
    float D = 1.0f - (ni_over_nt * ni_over_nt) * (1.0f - dt * dt);
    if (D > 0.0f)
    {
        refracted = -ni_over_nt * (uv - n * dt) - n * sqrt(D);
        return true;
    }
    else
    {
        return false;
    }
}

struct DielectricInfo
{
    float3 reflectedDir;
    float3 refractedDir;
    float reflectProb;
    bool canRefract;
    float3 attenuation;
};

DielectricInfo GetDielectricInfo(float3 rayDir, float3 normal, float refractiveIndex, inout uint seed)
{
    DielectricInfo info;
    info.attenuation = float3(1.0f, 1.0f, 1.0f);
    
    float3 outward_normal;
    float ni_over_nt;
    float cosine;
    
    info.reflectedDir = reflect_vec(rayDir, normal);
    
    if (dot(rayDir, normal) > 0.0f)
    {
        outward_normal = -normal;
        ni_over_nt = refractiveIndex;
        cosine = refractiveIndex * dot(rayDir, normal) / length(rayDir);
    }
    else
    {
        outward_normal = normal;
        ni_over_nt = 1.0f / refractiveIndex;
        cosine = -dot(rayDir, normal) / length(rayDir);
    }
    
    info.canRefract = refract_vec(-rayDir, outward_normal, ni_over_nt, info.refractedDir);
    
    if (info.canRefract)
    {
        info.reflectProb = schlick(cosine, refractiveIndex);
    }
    else
    {
        info.reflectProb = 1.0f;
    }
    
    return info;
}

BRDFSample SampleDielectricSurfaceBRDF(float3 normal, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    float r1 = RandomFloat(seed);
    float r2 = RandomFloat(seed);
    
    float z = sqrt(1.0f - r2);
    float phi = 2.0f * PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    
    float3 localDir = float3(x, y, z);
    
    float3 up = abs(normal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    sample.direction = localDir.x * tangent + localDir.y * bitangent + localDir.z * normal;
    
    sample.brdf = material.albedo * 0.05f / PI;
    sample.pdf = z / PI;
    sample.valid = true;
    
    return sample;
}

float3 CalculateDirectLighting(float3 worldPos, float3 normal, MaterialData material, inout uint seed)
{
    float3 directLighting = 0.0f;
    
    if (numLights == 0)
        return directLighting;
    
    uint maxLightsToSample = min(numLights, 2u);
    
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
                    float3 surfaceBrdf = material.albedo * 0.05f / PI;
                    float3 directContribution = surfaceBrdf * lightSample.radiance * NdotL;
                    
                    float brdfPdf = NdotL / PI;
                    
                    float misWeight = MISWeightLight(lightSample.pdf, brdfPdf);
                    directLighting += directContribution * misWeight / lightSample.pdf;
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

float3 CalculateIndirectSurfaceLighting(float3 worldPos, float3 normal, MaterialData material,
                                        uint depth, inout uint seed)
{
    float3 indirectLighting = 0.0f;
    
    if (depth >= 6)
        return indirectLighting;
    
    BRDFSample brdfSample = SampleDielectricSurfaceBRDF(normal, material, seed);
    
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
            float3 surfaceContribution = brdfSample.brdf * newPayload.color * NdotL;
            
            LightInfo light = GetLightInfo();
            float3 lightDir = normalize(light.position - worldPos);
            float lightDist = length(light.position - worldPos);
            float cosTheta = max(0.0f, dot(-lightDir, light.normal));
            float lightPdf = (lightDist * lightDist) / (cosTheta * light.area);
            
            float misWeight = MISWeightBRDF(lightPdf, brdfSample.pdf);
            indirectLighting += surfaceContribution * misWeight / brdfSample.pdf;
        }
    }
    
    return indirectLighting;
}

float3 CalculateRefractiveReflectiveLighting(float3 worldPos, float3 normal, MaterialData material,
                                           float3 rayDir, uint depth, inout uint seed)
{
    float3 refractiveReflectiveLighting = 0.0f;
    
    if (depth >= 8)
        return refractiveReflectiveLighting;
    
    DielectricInfo info = GetDielectricInfo(rayDir, normal, material.refractiveIndex, seed);
    
    // ã¸ê‹Ç≈Ç´Ç»Ç¢èÍçáÇÕïKÇ∏îΩéÀ
    bool useReflection = !info.canRefract || (RandomFloat(seed) < info.reflectProb);
    float3 chosenDirection = useReflection ? info.reflectedDir : info.refractedDir;
    
    if (length(chosenDirection) > 0.001f)
    {
        RayDesc ray;
        
        if (useReflection)
        {
            ray.Origin = worldPos + normal * 0.001f;
        }
        else
        {
            float3 offsetDir = dot(chosenDirection, normal) > 0 ? normal : -normal;
            ray.Origin = worldPos + offsetDir * 0.001f;
        }
        
        ray.Direction = normalize(chosenDirection);
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
        
        refractiveReflectiveLighting = info.attenuation * newPayload.color;
    }
    
    return refractiveReflectiveLighting;
}

[shader("closesthit")]
void ClosestHit_Dielectric(inout RayPayload payload, in VertexAttributes attr)
{
    bool dielectricDebugMode = false;
    if (dielectricDebugMode && payload.depth == 0)
    {
        payload.color = float3(1.0f, 1.0f, 0.0f);
        return;
    }
    
    if (payload.depth >= 4)
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
    
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_DIELECTRIC, material.roughness, RayTCurrent());
    
    
    float3 directLighting = CalculateDirectLighting(worldPos, normal, material, payload.seed);
    
    float3 indirectSurfaceLighting = CalculateIndirectSurfaceLighting(worldPos, normal, material,
                                                                      payload.depth, payload.seed);
    
    float3 refractiveReflectiveLighting = CalculateRefractiveReflectiveLighting(worldPos, normal, material,
                                                                               rayDir, payload.depth, payload.seed);
    
    payload.color = refractiveReflectiveLighting;
}