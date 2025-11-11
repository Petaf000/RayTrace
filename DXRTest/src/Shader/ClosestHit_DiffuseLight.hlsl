#include "Common.hlsli"

[shader("closesthit")]
void ClosestHit_DiffuseLight(inout RayPayload payload, in VertexAttributes attr)
{
    bool lightDebugMode = false;
    if (lightDebugMode && payload.depth == 0) {
        payload.color = float3(1.0f, 0.5f, 0.0f);        return;
    }
    
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    float3 rayDir = normalize(WorldRayDirection());
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    SetGBufferData(payload, worldPos, normal, material.emission,
                   MATERIAL_LIGHT, 0.0f, RayTCurrent());
    
    if (dot(rayDir, normal) < 0.0f)
    {
        if (payload.depth == 0)
        {
            payload.color = material.emission;
        }
        else
        {
            payload.color = material.emission;
        }
    }
    else
    {
        payload.color = float3(0, 0, 0);
    }
}