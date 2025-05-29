//=========================================
// shader/RayGen.hlsl - Ray Generation Shader (修正版)
#include "Common.hlsli"

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);
    
    // 正規化座標に変換 (-1 to 1)
    float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
    d.y = -d.y; // Y軸反転
    
    // ★ カメラパラメータ ★
    float3 cameraPos = viewMatrix._m03_m13_m23;
    float3 cameraUp = float3(0.0f, 1.0f, 0.0f);
    
    // カメラの基底ベクトル計算
    float3 forward = viewMatrix._m02_m12_m22;
    float3 right = normalize(viewMatrix._m00_m10_m20);
    float3 up = normalize(viewMatrix._m01_m11_m21);
    
    // FOV
    float cotHalfFov = projectionMatrix._22;
    float tanHalfFov = 1.0f / cotHalfFov;
    float fov = 2.0f * atan(tanHalfFov);
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
    
    // レイペイロード初期化
    RayPayload payload;
    payload.color = float3(0.2f, 0.4f, 0.8f); // デフォルトは青（Miss）
    payload.depth = 0;
    payload.seed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
    
    // ★ レイトレーシング実行 ★
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
    
    // 結果を出力
    RenderTarget[launchIndex.xy] = float4(payload.color, 1.0f);
    
    /*uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);
    
    // スクリーン座標を正規化座標に変換 (-1 to 1)
    float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
    d.y = -d.y; // Y軸反転
    
    // ★ 修正：正しいレイ生成 ★
    RayDesc ray;
    ray.Origin = cameraPosition.xyz;
    
    // ビュープロジェクション行列の逆行列を使用してワールド座標を計算
    // この部分は定数バッファに逆行列を追加するか、シェーダー内で計算
    
    // 簡単な方法：カメラパラメータから直接計算
    float3 cameraPos = cameraPosition.xyz;
    float3 cameraTarget = float3(278.0f, 278.0f, 278.0f); // ターゲット位置（固定）
    float3 cameraUp = float3(0.0f, 1.0f, 0.0f);
    
    // カメラの基底ベクトル計算
    float3 forward = normalize(cameraTarget - cameraPos);
    float3 right = normalize(cross(forward, cameraUp));
    float3 up = cross(right, forward);
    
    // FOVとアスペクト比（定数として設定）
    float fov = radians(40.0f);
    float aspect = dims.x / dims.y;
    float tanHalfFov = tan(fov * 0.5f);
    
    // レイ方向計算
    float3 rayDir = normalize(forward +
                            d.x * right * tanHalfFov * aspect +
                            d.y * up * tanHalfFov);
    
    ray.Direction = rayDir;
    ray.TMin = 0.1f; // 少し大きめの値
    ray.TMax = 10000.0f;
    
    // レイペイロード初期化
    RayPayload payload;
    payload.color = float3(1, 0, 1); // デバッグ用マゼンタ
    payload.depth = 0;
    payload.seed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
    
    // レイトレース実行
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
    
    // 結果を出力
    RenderTarget[launchIndex.xy] = float4(payload.color, 1.0f);
*/
}