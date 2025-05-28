//=========================================
// shader/RayGen.hlsl - Ray Generation Shader
#include "Common.hlsli"

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);
    
    // スクリーン座標を正規化座標に変換
    float2 d = ((crd / dims) * 2.0f - 1.0f);
    d.y = -d.y; // Y軸反転
    
    // レイ生成
    float4 origin = mul(float4(0, 0, 0, 1), viewProjectionMatrix);
    float4 target = mul(float4(d.x, d.y, 1, 1), viewProjectionMatrix);
    
    RayDesc ray;
    ray.Origin = cameraPosition.xyz;
    ray.Direction = normalize(target.xyz - origin.xyz);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;
    
    // レイペイロード初期化
    RayPayload payload;
    payload.color = float3(0, 0, 0);
    payload.depth = 0;
    payload.seed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
    
    // レイトレース実行
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
    
    // 結果を出力
    RenderTarget[launchIndex.xy] = float4(payload.color, 1.0f);
}