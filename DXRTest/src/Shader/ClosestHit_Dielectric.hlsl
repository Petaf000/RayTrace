// ===== ClosestHit_Dielectric.hlsl NEE+MIS対応版 =====
#include "Common.hlsli"

// CPUでのUtil.hと完全に同じSchlick近似
float schlick(float cosine, float ri)
{
    float r0 = (1.0f - ri) / (1.0f + ri);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

// CPUでのUtil.hと完全に同じreflect関数
float3 reflect_vec(float3 v, float3 n)
{
    return v - 2.0f * dot(v, n) * n;
}

// CPUでのUtil.hと完全に同じrefract関数
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

// CPUでのDielectric::scatterと完全に同じロジック
bool DielectricScatter(float3 rayDir, float3 normal, float refractiveIndex, inout uint seed, out float3 scatteredDir, out float3 attenuation)
{
    float3 outward_normal;
    float3 reflected = reflect_vec(rayDir, normal);
    float ni_over_nt;
    float reflect_prob;
    float cosine;
    
    // CPUでと完全に同じ処理
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
    
    float3 refracted;
    if (refract_vec(rayDir, outward_normal, ni_over_nt, refracted))
    {
        reflect_prob = schlick(cosine, refractiveIndex);
    }
    else
    {
        reflect_prob = 1.0f;
    }
    
    // ランダム選択による反射/屈折決定
    if (RandomFloat(seed) < reflect_prob)
    {
        scatteredDir = reflected;
    }
    else
    {
        scatteredDir = refracted;
    }
    
    attenuation = float3(1.0f, 1.0f, 1.0f); // 完全透明
    return true;
}

[shader("closesthit")]
void ClosestHit_Dielectric(inout RayPayload payload, in VertexAttributes attr)
{
    // デバッグモード：Dielectricシェーダー動作確認
    bool dielectricDebugMode = false;
    if (dielectricDebugMode && payload.depth == 0) {
        payload.color = float3(1.0f, 1.0f, 0.0f); // 黄色：Dielectricシェーダー実行
        return;
    }
    // 最大反射回数チェック
    if (payload.depth >= 4) // Dielectricは深い再帰が必要
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    // セカンダリレイでも通常処理を行う（簡略化を除去）
    
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    // 衝突点計算
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // 正確な法線を頂点データから取得
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    float3 rayDir = normalize(WorldRayDirection());
    
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_DIELECTRIC, material.roughness, RayTCurrent());
    
    // Dielectric材質でのレンダリング（NEE+MIS対応）
    float3 finalColor = 0.0f;
    
    // Dielectric材質はほぼ透明なので、NEEの寄与は非常に小さい
    // 学術論文では「完全鏡面反射後はMIS不要」とされているため、NEEは省略
    // しかし完全性のため、微小な拡散成分があると仮定してNEEを追加（オプション）
    
    bool useNEE = false; // Dielectricでは通常NEE不要
    
    if (useNEE && numLights > 0 && payload.depth > 0) {
        // 非常に小さな拡散成分があると仮定（現実的ではないが理論的完全性のため）
        uint maxLightsToSample = min(numLights, 1u);
        
        for (uint lightIdx = 0; lightIdx < maxLightsToSample; lightIdx++) {
            LightSample lightSample = SampleLightByIndex(lightIdx, worldPos, payload.seed);
            
            if (lightSample.valid) {
                float NdotL = max(0.0f, dot(normal, lightSample.direction));
                
                if (NdotL > 0.0f) {
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
                    
                    if (length(shadowPayload.color) > 0.5f) {
                        // 微小な拡散成分（非現実的だが理論的完全性のため）
                        float3 tinyDiffuse = material.albedo * 0.01f; // 1%の拡散成分
                        float3 directContribution = tinyDiffuse * lightSample.radiance * NdotL / PI;
                        finalColor += directContribution / lightSample.pdf;
                    }
                }
            }
        }
    }
    
    // Dielectric材質の主要処理：反射/屈折レイトレース
    if (payload.depth < 8) {
        float3 scatteredDir;
        float3 attenuation;
        
        if (DielectricScatter(rayDir, normal, material.refractiveIndex, payload.seed, scatteredDir, attenuation)) {
            // 反射または屈折レイトレース
            RayDesc ray;
            ray.Origin = OffsetRay(worldPos, normal);
            ray.Direction = scatteredDir;
            ray.TMin = 0.001f;
            ray.TMax = 1000.0f;
            
            RayPayload newPayload;
            newPayload.color = float3(0, 0, 0);
            newPayload.depth = payload.depth + 1;
            newPayload.seed = payload.seed;
            
            // G-Bufferデータ初期化
            newPayload.albedo = float3(0, 0, 0);
            newPayload.normal = float3(0, 0, 1);
            newPayload.worldPos = float3(0, 0, 0);
            newPayload.hitDistance = 0.0f;
            newPayload.materialType = 0;
            newPayload.roughness = 0.0f;
            newPayload.padding = 0;
            
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, newPayload);
            
            // 反射/屈折の寄与
            finalColor += attenuation * newPayload.color;
        }
    }
    
    payload.color = finalColor;
}