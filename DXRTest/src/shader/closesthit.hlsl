#include "raytrace.hlsli"

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    // 再帰の深さチェック - 光線の再帰的なバウンスを制限
    if (payload.recursionDepth >= 5)
    {
        payload.color = float4(0, 0, 0, 1);
        return;
    }
    
    // オブジェクトとプリミティブの識別情報を取得
    uint instanceID = InstanceID();
    uint primitiveIndex = PrimitiveIndex();
    
    // バリセントリック座標の計算
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    
    // 三角形の頂点インデックスを計算
    uint indexBase = 3 * primitiveIndex;
    
    // 三角形の頂点を取得
    float3 v0 = Vertices[indexBase];
    float3 v1 = Vertices[indexBase + 1];
    float3 v2 = Vertices[indexBase + 2];
    
    // 法線を計算
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 triangleNormal = normalize(cross(edge1, edge2));
    
    // オブジェクト空間からワールド空間への変換
    float4x3 objectToWorld = ObjectToWorld4x3();
    float3 worldNormal = normalize(mul(triangleNormal, (float3x3) objectToWorld));
    
    // 交差位置の計算
    float3 hitPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    
    // オブジェクトのマテリアルを取得
    MaterialData material = Materials[instanceID];
    
    // マテリアルタイプに基づく処理
    // 1. 発光マテリアル - 直接色を返す
    if (length(material.emissiveColor.rgb) > 0.0f)
    {
        payload.color = material.emissiveColor;
        return;
    }
    
    // 2. マテリアル別の処理
    if (material.metallic > 0.5f) // メタル (鏡面反射)
    {
        // 反射方向を計算
        float3 reflectDir = reflect(WorldRayDirection(), worldNormal);
        
        // ラフネスに基づくランダム化
        reflectDir = RandomReflectionDirection(reflectDir, worldNormal, material.roughness, payload.seed);
        
        // 反射レイを設定
        RayDesc reflectionRay;
        reflectionRay.Origin = hitPosition + worldNormal * 0.001f; // 自己交差を避けるためのオフセット
        reflectionRay.Direction = reflectDir;
        reflectionRay.TMin = 0.001f;
        reflectionRay.TMax = 100.0f;
        
        // 新しいペイロードを準備
        RayPayload reflectionPayload;
        reflectionPayload.color = float4(0, 0, 0, 0);
        reflectionPayload.seed = payload.seed;
        reflectionPayload.recursionDepth = payload.recursionDepth + 1;
        
        // 反射レイを発射
        TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            reflectionRay,
            reflectionPayload
        );
        
        // 金属の色を適用
        float3 reflectionColor = reflectionPayload.color.rgb * material.baseColor.rgb;
        payload.color = float4(reflectionColor, 1.0f);
    }
    else if (material.ior > 1.0f) // 誘電体 (透明/屈折)
    {
        // 屈折率と入射角を計算
        float cosine = dot(-WorldRayDirection(), worldNormal);
        float etai_over_etat = (cosine > 0.0f) ? (1.0f / material.ior) : material.ior;
        float3 outwardNormal = (cosine > 0.0f) ? worldNormal : -worldNormal;
        
        // 反射方向と屈折方向を計算
        float3 reflectDir = reflect(WorldRayDirection(), worldNormal);
        float3 refractDir;
        
        // 反射/屈折の決定
        float reflectProb;
        if (Refract(WorldRayDirection(), outwardNormal, etai_over_etat, refractDir))
        {
            // フレネル項を計算
            reflectProb = SchlickFresnel(cosine, material.ior);
        }
        else
        {
            // 全反射
            reflectProb = 1.0f;
        }
        
        // 確率に基づいて反射または屈折を選択
        float3 rayDir;
        float3 origin;
        
        if (Random(payload.seed) < reflectProb)
        {
            // 反射
            rayDir = reflectDir;
            origin = hitPosition + worldNormal * 0.001f;
        }
        else
        {
            // 屈折
            rayDir = refractDir;
            origin = hitPosition - worldNormal * 0.001f;
        }
        
        // 次のレイを設定
        RayDesc nextRay;
        nextRay.Origin = origin;
        nextRay.Direction = rayDir;
        nextRay.TMin = 0.001f;
        nextRay.TMax = 100.0f;
        
        // 新しいペイロードを準備
        RayPayload nextPayload;
        nextPayload.color = float4(0, 0, 0, 0);
        nextPayload.seed = payload.seed;
        nextPayload.recursionDepth = payload.recursionDepth + 1;
        
        // レイを発射
        TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            nextRay,
            nextPayload
        );
        
        // 色を透明材質の色でフィルタリング
        float3 finalColor = nextPayload.color.rgb * material.baseColor.rgb;
        payload.color = float4(finalColor, 1.0f);
    }
    else // ランバート (拡散反射)
    {
        // 拡散反射方向を生成
        float3 diffuseDir = RandomHemisphereDirection(worldNormal, payload.seed);
        
        // 拡散反射レイを設定
        RayDesc diffuseRay;
        diffuseRay.Origin = hitPosition + worldNormal * 0.001f; // 自己交差を避けるためのオフセット
        diffuseRay.Direction = diffuseDir;
        diffuseRay.TMin = 0.001f;
        diffuseRay.TMax = 100.0f;
        
        // 新しいペイロードを準備
        RayPayload diffusePayload;
        diffusePayload.color = float4(0, 0, 0, 0);
        diffusePayload.seed = payload.seed;
        diffusePayload.recursionDepth = payload.recursionDepth + 1;
        
        // 拡散反射レイを発射
        TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            diffuseRay,
            diffusePayload
        );
        
        // 拡散反射の効果を適用
        float NdotL = max(0.0f, dot(worldNormal, diffuseDir));
        float3 diffuseColor = diffusePayload.color.rgb * material.baseColor.rgb * NdotL;
        
        payload.color = float4(diffuseColor, 1.0f);
    }
}