#include "Common.hlsli"

[shader("closesthit")]
void ClosestHit_DiffuseLight(inout RayPayload payload, in VertexAttributes attr)
{
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    // ・・・ ライトマテリアル：発光のみ ・・・
    // ライトは光を発するだけで、他のレイは反射・屈折しない
    
    // 法線チェック（表面からのレイのみ発光）
    float3 rayDir = normalize(WorldRayDirection());
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetInterpolatedNormal(instanceID, primitiveID, attr.barycentrics);
    
    // 表面からのレイのみ発光（裏面は発光しない）
    if (dot(rayDir, normal) < 0.0f)
    {
        // プライマリレイの場合は強く光る
        if (payload.depth == 0)
        {
            payload.color = material.emission;
        }
        else
        {
            // 間接照明での光源の寄与（少し弱める）
            payload.color = material.emission * 0.8f;
        }
    }
    else
    {
        // 裏面は発光しない
        payload.color = float3(0, 0, 0);
    }
}