//=========================================
// shader/ClosestHit_Lambertian.hlsl - 汎用版
#include "Common.hlsli"
// マテリアルデータ（ローカルルートシグネチャから）
ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);
[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    // 最大深度チェック
    if (payload.depth >= 4)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    // ヒット点計算
    float3 hitPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // プリミティブインデックスから面を判定
    uint primitiveIndex = PrimitiveIndex();
    float3 normal;
    
    // ボックスの面に応じた法線
    uint faceIndex = primitiveIndex / 2; // 2つの三角形で1つの面
    
    switch (faceIndex)
    {
        case 0:
            normal = float3(0, 0, 1);
            break; // 前面 (Z+)
        case 1:
            normal = float3(0, 0, -1);
            break; // 背面 (Z-)
        case 2:
            normal = float3(1, 0, 0);
            break; // 右面 (X+)
        case 3:
            normal = float3(-1, 0, 0);
            break; // 左面 (X-)
        case 4:
            normal = float3(0, 1, 0);
            break; // 上面 (Y+)
        case 5:
            normal = float3(0, -1, 0);
            break; // 下面 (Y-)
        default:
            normal = float3(0, 0, 1);
            break; // デフォルト
    }
    
    // オブジェクト空間の法線をワールド空間に変換
    float4x3 objectToWorld = ObjectToWorld4x3();
    normal = normalize(mul((float3x3) objectToWorld, normal));
    
    // レイが裏面にヒットした場合は法線を反転
    if (dot(normal, -WorldRayDirection()) < 0.0f)
    {
        normal = -normal;
    }
    
    // 通常のLambertian反射処理
    // 深度0でもランバート反射を行う（本来の処理）
    // 通常のLambertian反射処理
    // ランバート反射
    uint tempSeed = payload.seed;
    float3 direction = RandomUnitVector(tempSeed);
    payload.seed = tempSeed;
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
    
    // 色の計算 - マテリアルのalbedoと散乱した光の色を合成
    float cosTheta = max(0.0f, dot(normal, scatterDirection));
    
    // 微量の環境光を追加（0.1倍）してコントラストを改善
    float3 ambient = MaterialCB.albedo * 0.1f;
    payload.color = MaterialCB.albedo * scatteredPayload.color * cosTheta + ambient;
}