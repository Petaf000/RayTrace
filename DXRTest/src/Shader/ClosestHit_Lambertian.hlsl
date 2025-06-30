// ===== ClosestHit_Lambertian.hlsl の修正 =====
#include "Common.hlsli"

// LAMBERTIAN拡散BRDF サンプリング
BRDFSample SampleLambertianBRDF(float3 normal, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    // コサイン重み付きサンプリング
    float r1 = RandomFloat(seed);
    float r2 = RandomFloat(seed);
    
    float cosTheta = sqrt(1.0f - r2);
    float sinTheta = sqrt(r2);
    float phi = 2.0f * PI * r1;
    
    // 局所座標系での方向
    float3 localDir = float3(
        sinTheta * cos(phi),
        sinTheta * sin(phi),
        cosTheta
    );
    
    // 法線基準の座標系に変換
    float3 up = abs(normal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    sample.direction = localDir.x * tangent + localDir.y * bitangent + localDir.z * normal;
    sample.brdf = material.albedo / PI; // Lambertian BRDF
    sample.pdf = cosTheta / PI; // コサイン重み付きPDF
    sample.valid = true;
    
    return sample;
}

[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    // 最大反射回数チェック
    if (payload.depth >= 6)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    uint instanceID = InstanceID();
    uint primitiveID = PrimitiveIndex();
    MaterialData material = GetMaterial(instanceID);
    
    // 交点を計算
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // 正確な法線をワールド空間で取得
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    // レイ方向と法線の向きを確認
    float3 rayDir = normalize(WorldRayDirection());
    if (dot(normal, rayDir) > 0.0f)
    {
        normal = -normal;
    }
    
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_LAMBERTIAN, material.roughness, RayTCurrent());
    
    // 発光成分
    float3 finalColor = 0.0f;
    
    // 1. 直接照明（Next Event Estimation）
    LightSample lightSample = SampleAreaLight(worldPos, payload.seed);
    
    if (lightSample.valid)
    {
        float NdotL = max(0.0f, dot(normal, lightSample.direction));
        
        if (NdotL > 0.0f)
        {
            // シャドウレイで可視性チェック
            RayDesc shadowRay;
            shadowRay.Origin = OffsetRay(worldPos, normal);
            shadowRay.Direction = lightSample.direction;
            shadowRay.TMin = 0.001f;
            shadowRay.TMax = lightSample.distance - 0.001f;
            
            RayPayload shadowPayload;
            shadowPayload.color = float3(1, 1, 1);
            shadowPayload.depth = 999;
            shadowPayload.seed = payload.seed;
            
            TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                     0xFF, 0, 1, 0, shadowRay, shadowPayload);
            
            if (length(shadowPayload.color) > 0.5f)
            {
                // 直接照明の寄与
                float3 brdf = material.albedo / PI;
                finalColor += brdf * lightSample.radiance * NdotL / lightSample.pdf;
            }
        }
    }
    
    // 2. 間接照明（簡略化 - 最初の数回のみ）
    if (payload.depth < 3) // 計算コストを抑えるため最初の数回のみ
    {
        BRDFSample brdfSample = SampleLambertianBRDF(normal, material, payload.seed);
        
        if (brdfSample.valid)
        {
            // 間接照明レイをトレース
            RayDesc ray;
            ray.Origin = OffsetRay(worldPos, normal);
            ray.Direction = brdfSample.direction;
            ray.TMin = 0.001f;
            ray.TMax = 1000.0f;
            
            RayPayload newPayload;
            newPayload.color = float3(0, 0, 0);
            newPayload.depth = payload.depth + 1;
            newPayload.seed = payload.seed;
            
            // G-Bufferデータを初期化（セカンダリレイ用）
            newPayload.albedo = float3(0, 0, 0);
            newPayload.normal = float3(0, 0, 1);
            newPayload.worldPos = float3(0, 0, 0);
            newPayload.hitDistance = 0.0f;
            newPayload.materialType = 0;
            newPayload.roughness = 0.0f;
            newPayload.padding = 0;
            
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, newPayload);
            
            // 間接照明の寄与（適度な重み）
            finalColor += material.albedo * newPayload.color * 0.5f;
        }
    }
    
    payload.color = finalColor;
}