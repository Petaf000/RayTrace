//=========================================
// shader/ClosestHit_DiffuseLight.hlsl - 発光マテリアル
#include "Common.hlsli"
// マテリアルデータ（ローカルルートシグネチャから）
ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);

[shader("closesthit")]
void ClosestHit_DiffuseLight(inout RayPayload payload, in VertexAttributes attr)
{
    // 発光マテリアルは光源なので、emission色を直接返す
    payload.color = MaterialCB.emission;
}