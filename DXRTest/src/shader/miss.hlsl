#include "raytrace.hlsli"

// ミスシェーダー
[shader("miss")]
void MissShader(inout RayPayload payload)
{
    float3 rayDir = WorldRayDirection();
    
    float t = rayDir.y * 0.5f + 0.5f;
    
    float3 skyColor = lerp(float3(1.0f, 1.0f, 1.0f), float3(0.5f, 0.7f, 1.0f), t);
    
    skyColor *= 0.3f;
    
    payload.color = float4(skyColor, 1.0f);
}