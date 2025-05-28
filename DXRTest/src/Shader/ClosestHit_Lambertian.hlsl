//=========================================
// shader/ClosestHit_Lambertian.hlsl - Lambertian Material
#include "Common.hlsli"

// マテリアルデータ（ローカルルートシグネチャから）
ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);

[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    // 最大深度チェック
    if (payload.depth >= 3)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    // ヒット点と法線計算
    float3 hitPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // TODO: 正確な法線計算（頂点データから補間）
    // 現在は簡易的にレイ方向の逆を使用
    float3 normal = -normalize(WorldRayDirection());
    
    // ランバート反射
    uint tempSeed = payload.seed;
    float3 direction = RandomUnitVector(tempSeed);
    payload.seed = tempSeed; // 更新
    float3 scatterDirection = normalize(normal + direction);
    
    // 新しいレイの生成
    RayDesc scatteredRay;
    scatteredRay.Origin = OffsetRay(hitPoint, normal);
    scatteredRay.Direction = scatterDirection;
    scatteredRay.TMin = 0.001f;
    scatteredRay.TMax = 10000.0f;
    
    // 再帰的レイトレース
    RayPayload scatteredPayload;
    scatteredPayload.color = float3(0, 0, 0);
    scatteredPayload.depth = payload.depth + 1;
    scatteredPayload.seed = payload.seed;
    
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, scatteredRay, scatteredPayload);
    
    // 色の計算
    payload.color = MaterialCB.albedo * scatteredPayload.color * dot(normal, scatterDirection);
}