#include "Common.hlsli"

// 追加のG-Buffer出力用 (既存のu0に加えて)
RWTexture2D<float4> AlbedoOutput : register(u1);
RWTexture2D<float4> NormalOutput : register(u2);
RWTexture2D<float4> DepthOutput : register(u3);

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    
    // アンチエイリアシング用マルチサンプリング
    float3 finalColor = float3(0, 0, 0);
    float3 finalAlbedo = float3(0, 0, 0);
    float3 finalNormal = float3(0, 0, 0);
    float finalDepth = 0.0f;
    uint finalMaterialType = 0;
    float finalRoughness = 0.0f;
    
    const int SAMPLES = 10; // サンプル数を調整
    
    for (int sampleIndex = 0; sampleIndex < SAMPLES; sampleIndex++)
    {
        // 各サンプルで異なるシード値を生成
        uint baseSeed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
        uint seed = WangHash(baseSeed + sampleIndex * 12345);
        
        // より強いジッター（フルピクセル内）
        float2 jitter = float2(RandomFloat(seed), RandomFloat(seed)) - 0.5f;
        float2 crd = float2(launchIndex.xy) + jitter; // フルジッター適用
        float2 dims = float2(launchDim.xy);
        
        // 正規化座標に変換 (-1 to 1)
        float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
        d.y = -d.y; // Y軸反転
        
        // カメラパラメータ
        float3 cameraPos = viewMatrix._m03_m13_m23;
        
        // カメラの向きベクトル計算
        float3 forward = viewMatrix._m02_m12_m22;
        float3 right = normalize(viewMatrix._m00_m10_m20);
        float3 up = normalize(viewMatrix._m01_m11_m21);
        
        // FOV計算
        float cotHalfFov = projectionMatrix._22;
        float tanHalfFov = 1.0f / cotHalfFov;
        float aspect = dims.x / dims.y;
        
        // レイ方向計算
        float3 rayDir = normalize(forward +
                                d.x * right * tanHalfFov * aspect +
                                d.y * up * tanHalfFov);
        
        // レイ設定
        RayDesc ray;
        ray.Origin = cameraPos;
        ray.Direction = rayDir;
        ray.TMin = 0.1f;
        ray.TMax = 10000.0f;
        
        // レイペイロード初期化（全フィールドを明示的に初期化）
        RayPayload payload;
        payload.color = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.depth = 0; // 4 bytes
        payload.seed = seed; // 4 bytes
        
        // G-Bufferデータ初期化（全フィールドを明示的に初期化）
        payload.albedo = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.normal = float3(0, 0, 1); // 4+4+4 = 12 bytes (デフォルト法線)
        payload.worldPos = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.hitDistance = 0.0f; // 4 bytes
        payload.materialType = 0; // 4 bytes
        payload.roughness = 0.0f; // 4 bytes
        payload.padding = 0; // 4 bytes
        
        // 合計: 72 bytes
        
        // レイトレーシング実行
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
        
        // サンプル結果を蓄積
        finalColor += payload.color;
        
        // プライマリレイのG-Bufferデータのみ蓄積
        if (sampleIndex == 0)  // ★★★ 最初のサンプルのみで判定を簡素化 ★★★
        {
            finalAlbedo += payload.albedo;
            finalNormal += payload.normal;
            finalDepth += payload.hitDistance;
            finalMaterialType = payload.materialType;
            finalRoughness += payload.roughness;
        }
    }
    
    // 平均化
    finalColor /= float(SAMPLES);
    finalAlbedo /= float(SAMPLES);
    finalNormal /= float(SAMPLES);
    finalDepth /= float(SAMPLES);
    finalRoughness /= float(SAMPLES);
    
    // 法線の正規化
    if (length(finalNormal) > 0.1f)
    {
        finalNormal = normalize(finalNormal);
    }
    else
    {
        finalNormal = float3(0, 0, 1); // デフォルト法線
    }
    
    // ポストプロセシング
    float3 color = finalColor;
    
    // NaNチェック
    if (any(isnan(color)) || any(isinf(color)))
    {
        color = float3(1, 0, 1); // マゼンタでエラー表示
    }
    
    // より自然なトーンマッピング（記事の技法を参考）
    color = color / (0.8f + color); // 少し控えめに
    
    // 色の彩度を少し上げる
    float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    color = lerp(float3(luminance, luminance, luminance), color, 1.2f);
    
    // ガンマ補正
    color = pow(max(color, 0.0f), 1.0f / 2.2f);
    
    // G-Bufferのガンマ補正とNaNチェック
    if (any(isnan(finalAlbedo)) || any(isinf(finalAlbedo)))
        finalAlbedo = float3(0, 0, 0);
    if (any(isnan(finalNormal)) || any(isinf(finalNormal)))
        finalNormal = float3(0, 0, 1); // デフォルト法線
    if (isnan(finalDepth) || isinf(finalDepth))
        finalDepth = 0.0f;
    
    // ★★★ 修正：深度値を正規化して可視化 ★★★
    const float MAX_VIEW_DEPTH = 1000.0f; // カメラのFarクリップなどに合わせる
    float normalizedDepth = saturate(finalDepth / MAX_VIEW_DEPTH);

    // 結果を出力
    RenderTarget[launchIndex.xy] = float4(color, 1.0f);
    AlbedoOutput[launchIndex.xy] = float4(finalAlbedo, finalRoughness);
    NormalOutput[launchIndex.xy] = float4(finalNormal, 1.0f);
    // ★★★ 修正：正規化された深度を書き込む ★★★
    DepthOutput[launchIndex.xy] = float4(normalizedDepth, normalizedDepth, normalizedDepth, 1.0f);
}