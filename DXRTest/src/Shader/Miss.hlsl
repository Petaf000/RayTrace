//=========================================
// shader/Miss.hlsl - Miss Shader
#include "Common.hlsli"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    // ƒVƒ“ƒvƒ‹‚ÈÂ‚¢”wŒi
    payload.color = float3(0.2f, 0.4f, 0.8f);
}