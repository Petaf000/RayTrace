//=========================================
// shader/ClosestHit_Lambertian.hlsl - 最終版（マテリアル色表示）
#include "Common.hlsli"
// マテリアルデータ（ローカルルートシグネチャから）
ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);
[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    if (payload.depth == 0)
    {
        // ★強制的に赤色を設定
        payload.color = float3(0.65f, 0.05f, 0.05f); // CornelBoxSceneで設定した赤色
        return;
    }
    
    payload.color = float3(0, 0, 0);
}