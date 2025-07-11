// ===== ClosestHit_Dielectric.hlsl 完全書き直し版 =====
#include "Common.hlsli"

// CPU版のUtil.hと完全に同じSchlick近似
float schlick(float cosine, float ri)
{
    float r0 = (1.0f - ri) / (1.0f + ri);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

// CPU版のUtil.hと完全に同じreflect関数
float3 reflect_vec(float3 v, float3 n)
{
    return v - 2.0f * dot(v, n) * n;
}

// CPU版のUtil.hと完全に同じrefract関数
bool refract_vec(float3 v, float3 n, float ni_over_nt, out float3 refracted)
{
    float3 uv = normalize(v);
    float dt = dot(uv, n);
    float D = 1.0f - (ni_over_nt * ni_over_nt) * (1.0f - dt * dt);
    if (D > 0.0f)
    {
        refracted = -ni_over_nt * (uv - n * dt) - n * sqrt(D);
        return true;
    }
    else
    {
        return false;
    }
}

// CPU版のDielectric::scatterと完全に同じロジック
bool DielectricScatter(float3 rayDir, float3 normal, float refractiveIndex, inout uint seed, out float3 scatteredDir, out float3 attenuation)
{
    float3 outward_normal;
    float3 reflected = reflect_vec(rayDir, normal);
    float ni_over_nt;
    float reflect_prob;
    float cosine;
    
    // CPU版と完全に同じ判定
    if (dot(rayDir, normal) > 0.0f)
    {
        outward_normal = -normal;
        ni_over_nt = refractiveIndex;
        cosine = refractiveIndex * dot(rayDir, normal) / length(rayDir);
    }
    else
    {
        outward_normal = normal;
        ni_over_nt = 1.0f / refractiveIndex;
        cosine = -dot(rayDir, normal) / length(rayDir);
    }
    
    // CPU版と同じアルベド設定
    attenuation = float3(1.0f, 1.0f, 1.0f);
    
    float3 refracted;
    if (refract_vec(-rayDir, outward_normal, ni_over_nt, refracted))
    {
        reflect_prob = schlick(cosine, refractiveIndex);
    }
    else
    {
        reflect_prob = 1.0f;
    }
    
    // CPU版のdrand48()と同じ確率判定
    if (RandomFloat(seed) < reflect_prob)
    {
        scatteredDir = reflected;
    }
    else
    {
        scatteredDir = refracted;
    }
    
    return true;
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
    
    // CPU版と同じくWorldRayDirection()をそのまま使用（正規化しない）
    float3 rayDir = WorldRayDirection();
    
    // G-Bufferデータを設定
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   2, material.roughness, RayTCurrent()); // 2 = MATERIAL_DIELECTRIC
    
    // CPU版と完全に同じDielectricの散乱計算
    float3 scatteredDir;
    float3 attenuation;
    
    if (DielectricScatter(rayDir, normal, material.refractiveIndex, payload.seed, scatteredDir, attenuation))
    {
        // 散乱レイをトレース
        RayDesc ray;
        ray.Origin = worldPos; // CPU版と同じく単純な開始点
        ray.Direction = normalize(scatteredDir); // 方向を正規化
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
        
        // CPU版と同じく背面カリングなし
        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, newPayload);
        
        // CPU版と同じく、attenuation（常に白）を掛ける
        payload.color = attenuation * newPayload.color;
    }
    else
    {
        payload.color = float3(0, 0, 0);
    }
}