//=========================================
// shader/Miss.hlsl - Miss Shader
#include "Common.hlsli"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    // �V���v���Ȑ��w�i
    payload.color = float3(0.2f, 0.4f, 0.8f);
}