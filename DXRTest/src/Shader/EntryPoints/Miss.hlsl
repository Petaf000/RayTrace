#include "../Common.hlsli"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    if (payload.depth == 999)
    {
        payload.color = float3(1, 1, 1);
        return;
    }
    
    
    if (payload.depth == 0)
    {
        payload.color = float3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        payload.color = float3(0.0f, 0.0f, 0.0f);
    }
}