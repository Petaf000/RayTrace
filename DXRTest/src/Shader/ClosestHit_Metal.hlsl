//=========================================
// shader/ClosestHit_Metal.hlsl - Metal Material
#include "Common.hlsli"

ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);

[shader("closesthit")]
void ClosestHit_Metal(inout RayPayload payload, in VertexAttributes attr)
{
    payload.color = float3(0.0f, 1.0f, 0.0f); // ê‘
    return;
    if (payload.depth >= 4)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    float3 hitPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 normal = -normalize(WorldRayDirection()); // ä»à’ñ@ê¸
    
    // ã‡ëÆîΩéÀ
    float3 reflected = reflect(normalize(WorldRayDirection()), normal);
    uint tempSeed = payload.seed;
    float3 direction = RandomUnitVector(tempSeed);
    payload.seed = tempSeed; // çXêV
    reflected += MaterialCB.roughness * direction;
    
    RayDesc reflectedRay;
    reflectedRay.Origin = OffsetRay(hitPoint, normal);
    reflectedRay.Direction = normalize(reflected);
    reflectedRay.TMin = 0.001f;
    reflectedRay.TMax = 10000.0f;
    
    RayPayload reflectedPayload;
    reflectedPayload.color = float3(0, 0, 0);
    reflectedPayload.depth = payload.depth + 1;
    reflectedPayload.seed = payload.seed;
    
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, reflectedRay, reflectedPayload);
    
    payload.color = MaterialCB.albedo * reflectedPayload.color;
}