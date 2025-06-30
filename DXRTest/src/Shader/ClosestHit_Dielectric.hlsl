// ===== ClosestHit_Dielectric.hlsl の修正 =====
#include "Common.hlsli"

// Schlick近似（Dielectric用）
float Schlick(float cosine, float refractiveIndex)
{
    float r0 = (1.0f - refractiveIndex) / (1.0f + refractiveIndex);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

// Dielectric専用サンプリング
BRDFSample SampleDielectricBRDF(float3 normal, float3 rayDir, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    float3 outwardNormal;
    float niOverNt;
    float cosine;
    
    // 内側から外側への判定
    if (dot(rayDir, normal) > 0)
    {
        // レイが内側から来ている
        outwardNormal = -normal;
        niOverNt = material.refractiveIndex;
        cosine = material.refractiveIndex * dot(rayDir, normal);
    }
    else
    {
        // レイが外側から来ている
        outwardNormal = normal;
        niOverNt = 1.0f / material.refractiveIndex;
        cosine = -dot(rayDir, normal);
    }
    
    float3 refracted;
    float reflectProb;
    
    // 屈折計算
    float discriminant = 1.0f - niOverNt * niOverNt * (1.0f - cosine * cosine);
    if (discriminant > 0)
    {
        refracted = niOverNt * (rayDir - outwardNormal * cosine) - outwardNormal * sqrt(discriminant);
        reflectProb = Schlick(cosine, material.refractiveIndex);
    }
    else
    {
        // 全反射
        reflectProb = 1.0f;
    }
    
    // 反射か屈折かを確率的に決定
    if (RandomFloat(seed) < reflectProb)
    {
        // 反射
        sample.direction = reflect(rayDir, outwardNormal);
    }
    else
    {
        // 屈折
        sample.direction = normalize(refracted);
    }
    
    sample.brdf = material.albedo; // 通常は白（透明）
    sample.pdf = 1.0f; // スペキュラー
    sample.valid = true;
    
    return sample;
}

[shader("closesthit")]
void ClosestHit_Dielectric(inout RayPayload payload, in VertexAttributes attr)
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
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    float3 rayDir = normalize(WorldRayDirection());
    
    // G-Bufferデータを設定（プライマリレイの場合のみ）
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_DIELECTRIC, material.roughness, RayTCurrent());
    
    // ガラス：屈折と反射の確率的選択
    // ガラスは基本的にスペキュラーなので、直接照明の計算は不要
    
    BRDFSample brdfSample = SampleDielectricBRDF(normal, rayDir, material, payload.seed);
    
    if (brdfSample.valid)
    {
        // 屈折または反射レイをトレース
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
        
        // ガラス色で調光（通常は透明なのでほぼ影響しない）
        payload.color = brdfSample.brdf * newPayload.color;
    }
    else
    {
        payload.color = float3(0, 0, 0);
    }
}