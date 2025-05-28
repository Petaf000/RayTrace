//=========================================
// shader/Miss.hlsl - Miss Shader
#include "Common.hlsli"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    // �w�i�F�i���j
    payload.color = float3(0.0f, 0.0f, 0.0f);
}