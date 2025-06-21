#include "Common.hlsli"

// ・・・ Metalマテリアル用BRDFサンプリング ・・・
BRDFSample SampleMetalBRDF(float3 normal, float3 rayDir, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    // 鏡面反射方向
    float3 reflected = reflect(rayDir, normal);
    
    // ラフネスによるランダム性を追加
    float3 perturbation = material.roughness * RandomUnitVector(seed);
    sample.direction = normalize(reflected + perturbation);
    
    // 法線との内積チェック
    float NdotL = dot(sample.direction, normal);
    if (NdotL > 0.0f)
    {
        sample.brdf = material.albedo; // 金属のBRDF
        sample.pdf = 1.0f; // デルタ関数的（スペキュラー）
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
    }
    
    return sample;
}

[shader("closesthit")]
void ClosestHit_Metal(inout RayPayload payload, in VertexAttributes attr)
{
    // 最大反射回数チェック
    if (payload.depth >= 6)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    // 交点を計算
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // 正確な法線を頂点データから取得
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetInterpolatedNormal(instanceID, primitiveID, attr.barycentrics);
    
    // レイ方向と法線の向きを確認
    float3 rayDir = normalize(WorldRayDirection());
    if (dot(normal, rayDir) > 0.0f)
    {
        normal = -normal;
    }
    
    // 発光成分
    float3 emitted = material.emission;
    
    // ・・・ スペキュラー材質：CPUレイトレと同じ処理 ・・・
    BRDFSample brdfSample = SampleMetalBRDF(normal, rayDir, material, payload.seed);
    
    if (brdfSample.valid)
    {
        // 反射レイをトレース
        RayDesc ray;
        ray.Origin = OffsetRay(worldPos, normal);
        ray.Direction = brdfSample.direction;
        ray.TMin = 0.001f;
        ray.TMax = 1000.0f;
        
        RayPayload newPayload;
        newPayload.color = float3(0, 0, 0);
        newPayload.depth = payload.depth + 1;
        newPayload.seed = payload.seed;
        
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, newPayload);
        
        // CPUレイトレと同じ：emitted + mulPerElem(albedo, color)
        payload.color = emitted + brdfSample.brdf * newPayload.color;
    }
    else
    {
        payload.color = emitted;
    }
}