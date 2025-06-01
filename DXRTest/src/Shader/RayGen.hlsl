#include "Common.hlsli"

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    // ・・・ より多いサンプル数でノイズ削減 ・・・
    float3 finalColor = float3(0, 0, 0);
    const int SAMPLES = 25; // サンプル数を増やす
    
    for (int sampleIndex = 0; sampleIndex < SAMPLES; sampleIndex++)
    {
        // 各サンプルで異なるシード値を生成
        uint baseSeed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
        uint seed = WangHash(baseSeed + sampleIndex * 12345);
        
        // より強いジッター（フルピクセル範囲）
        float2 jitter = float2(RandomFloat(seed), RandomFloat(seed)) - 0.5f;
        float2 crd = float2(launchIndex.xy) + jitter; // フルジッター適用
        float2 dims = float2(launchDim.xy);
        
        // 正規化座標に変換 (-1 to 1)
        float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
        d.y = -d.y; // Y軸反転
        
        // ・ カメラパラメータ ・
        float3 cameraPos = viewMatrix._m03_m13_m23;
        
        // カメラの基底ベクトル計算
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
        
        // ・・・ レイペイロード初期化 ・・・
        RayPayload payload;
        payload.color = float3(0, 0, 0);
        payload.depth = 0;
        payload.seed = seed; // サンプル固有のシード
        
        // ・ レイトレーシング実行 ・
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
        
        // サンプル結果を蓄積
        finalColor += payload.color;
    }
    
    // 平均化
    finalColor /= float(SAMPLES);
    
    // ・・・ ポストプロセッシング ・・・
    float3 color = finalColor;
    
    // NaNチェック
    if (any(isnan(color)) || any(isinf(color)))
    {
        color = float3(1, 0, 1); // マゼンタでエラー表示
    }
    
    // より自然なトーンマッピング（記事の手法を参考）
    color = color / (0.8f + color); // 少し控えめに
    
    // 色の彩度を少し上げる
    float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    color = lerp(float3(luminance, luminance, luminance), color, 1.2f);
    
    // ガンマ補正
    color = pow(max(color, 0.0f), 1.0f / 2.2f);
    
    // 結果を出力
    RenderTarget[launchIndex.xy] = float4(color, 1.0f);
}