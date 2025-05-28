//=========================================
// shader/ClosestHit_Dielectric.hlsl - Dielectric Material
#include "Common.hlsli"

ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);

float Schlick(float cosine, float refractiveIndex)
{
    float r0 = (1.0f - refractiveIndex) / (1.0f + refractiveIndex);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

[shader("closesthit")]
void ClosestHit_Dielectric(inout RayPayload payload, in VertexAttributes attr)
{
    if (payload.depth >= 3)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    float3 hitPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 normal = -normalize(WorldRayDirection()); // 簡易法線
    
    float3 rayDir = normalize(WorldRayDirection());
    float3 outwardNormal;
    float niOverNt;
    float cosine;
    
    // 内側から外側への判定
    if (dot(rayDir, normal) > 0)
    {
        outwardNormal = -normal;
        niOverNt = MaterialCB.refractiveIndex;
        cosine = MaterialCB.refractiveIndex * dot(rayDir, normal);
    }
    else
    {
        outwardNormal = normal;
        niOverNt = 1.0f / MaterialCB.refractiveIndex;
        cosine = -dot(rayDir, normal);
    }
    
    float3 refracted;
    float reflectProb;
    
    // 屈折計算
    float discriminant = 1.0f - niOverNt * niOverNt * (1.0f - cosine * cosine);
    if (discriminant > 0)
    {
        refracted = niOverNt * (rayDir - outwardNormal * cosine) - outwardNormal * sqrt(discriminant);
        reflectProb = Schlick(cosine, MaterialCB.refractiveIndex);
    }
    else
    {
        reflectProb = 1.0f;
    }
    
    RayDesc scatteredRay;
    scatteredRay.Origin = OffsetRay(hitPoint, outwardNormal);
    scatteredRay.TMin = 0.001f;
    scatteredRay.TMax = 10000.0f;
    
    // 反射か屈折かを確率的に決定
    if (RandomFloat(payload.seed) < reflectProb)
    {
        scatteredRay.Direction = reflect(rayDir, outwardNormal);
    }
    else
    {
        scatteredRay.Direction = normalize(refracted);
    }
    
    RayPayload scatteredPayload;
    scatteredPayload.color = float3(0, 0, 0);
    scatteredPayload.depth = payload.depth + 1;
    scatteredPayload.seed = payload.seed;
    
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, scatteredRay, scatteredPayload);
    
    payload.color = MaterialCB.albedo * scatteredPayload.color;
}