#include "Common.hlsli"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    // シャドウレイの場合
    if (payload.depth == 999)
    {
        payload.color = float3(1, 1, 1); // 光が届く
        return;
    }
    
    // ・・・ 環境光（記事を参考に自然な設定） ・・・
    
    // プライマリレイの場合：空の色
    if (payload.depth == 0)
    {
        // 暗い環境光（参考コードに合わせる）
        payload.color = float3(0.0f, 1.0f, 0.0f);
    }
    else
    {
        // 間接照明の場合：環境光
        payload.color = float3(0.0f, 0.0f, 0.0f);
    }
}