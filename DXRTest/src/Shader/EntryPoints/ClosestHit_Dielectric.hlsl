#include "../Common.hlsli"

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
    
    info.reflectedDir = ReflectVec(rayDir, normal);
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
    
    info.canRefract = RefractVec(-rayDir, outward_normal, ni_over_nt, info.refractedDir);
    
    if (info.canRefract)
    {
        info.reflectProb = SchlickFresnel(cosine, refractiveIndex);
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
                // シャドウレイ関数を使用
                if (TestLightVisibility(worldPos, normal, lightSample.direction, lightSample.distance, seed))
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
            
            // ここでは簡易的なMISの実装（本来はライト方向への再計算が必要だが既存ロジックを維持）
            if (numLights > 0)
            {
                LightInfo light = LightBuffer[0];
                float3 lightDir = normalize(light.position - worldPos);
                float lightDist = length(light.position - worldPos);
                float cosTheta = max(0.0f, dot(-lightDir, light.normal));
                float lightPdf = (lightDist * lightDist) / (cosTheta * light.area);
                
                float misWeight = MISWeightBRDF(lightPdf, brdfSample.pdf);
                indirectLighting += surfaceContribution * misWeight / brdfSample.pdf;
            }
            else
            {
                indirectLighting += surfaceContribution / brdfSample.pdf;
            }
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
    // 屈折できない場合は必ず反射
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
        
        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, newPayload);
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