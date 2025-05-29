//=========================================
// shader/ClosestHit_DiffuseLight.hlsl - Emissive Material
#include "Common.hlsli"

ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);

[shader("closesthit")]
void ClosestHit_DiffuseLight(inout RayPayload payload, in VertexAttributes attr)
{
    payload.color = float3(1, 1, 1);
    return;
    // 発光マテリアルは光を放出するのみ
    payload.color = MaterialCB.emission;
}