#include "raytrace.hlsli"

[shader("raygeneration")]
void RaygenShader()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    
    float2 d = (((launchIndex.xy + 0.5f) / launchDim.xy) * 2.f - 1.f);
    d.y = -d.y; // Y軸を反転
    
    // カメラパラメータ
    float3 origin = float3(0, 5, -10); // カメラ位置
    float3 lookAt = float3(0, 5, 0); // 注視点 (コーネルボックスの中心)
    float3 up = float3(0, 1, 0); // 上方向
    float fov = 60.0f * (3.14159f / 180.0f); // 視野角（ラジアン）
    
    // カメラ空間の構築
    float3 cameraZ = normalize(lookAt - origin); // 前方向
    float3 cameraX = normalize(cross(up, cameraZ)); // 右方向
    float3 cameraY = cross(cameraZ, cameraX); // 上方向の再計算
    
    // 画面のアスペクト比
    float aspectRatio = float(launchDim.x) / float(launchDim.y);
    
    // レイの方向計算
    float tanHalfFovy = tan(fov / 2.0f);
    float3 rayDir = normalize(
        cameraZ +
        cameraX * d.x * tanHalfFovy * aspectRatio +
        cameraY * d.y * tanHalfFovy
    );
    
    // 乱数シードの初期化
    float seed = float(launchIndex.x + launchIndex.y * launchDim.x) / float(launchDim.x * launchDim.y);
    seed = frac(seed * 1337.0f); // 初期シード
    
    // サンプル数（アンチエイリアシングやモンテカルロ用）
    const uint samplesPerPixel = 4;
    float4 finalColor = float4(0, 0, 0, 0);
    
    for (uint sample = 0; sample < samplesPerPixel; sample++)
    {
        // レイの作成
        RayDesc ray;
        ray.Origin = origin;
        ray.Direction = rayDir;
        ray.TMin = 0.001;
        ray.TMax = 100.0;
        
        // ペイロード初期化
        RayPayload payload;
        payload.color = float4(0, 0, 0, 0);
        payload.seed = seed + sample * 0.1f; // サンプルごとに異なるシード
        payload.recursionDepth = 0;
        
        
        
        // レイを発射
        TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            ray,
            payload
        );
        
        // 結果を蓄積
        finalColor += payload.color;
    }
    
    // 平均を取る
    finalColor /= float(samplesPerPixel);
    
    // トーンマッピングを適用
    finalColor = float4(float3(1.0,1.0,1.0) - exp(-finalColor.rgb), 1.0f);
    
    // ガンマ補正
    finalColor.rgb = pow(finalColor.rgb, 1.0f / 2.2f);
    
    // 結果を出力
    RenderTarget[launchIndex] = finalColor;
}