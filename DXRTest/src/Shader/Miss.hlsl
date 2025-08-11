#include "Common.hlsli"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    // シャドウレイの場合
    if (payload.depth == 999)
    {
        payload.color = float3(1, 1, 1); // 光は遮蔽されない
        return;
    }
    
    // 環境光設定（参考コードに従って自然な設定）
    
    // プライマリレイの場合：黒の色
    if (payload.depth == 0)
    {
        // 暗い背景（参考コードに合わせる）
        payload.color = float3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        // 間接照明の場合：黒色
        // 環境光なし：完全に黒色
        // **デバッグ: GIレイの場合は固定色を返す**
        if (payload.depth == 1) {
            payload.color = float3(0.0f, 0.0f, 1.0f); // GIレイミス時：青色
        } else {
            payload.color = float3(0.0f, 0.0f, 0.0f); // 黒：環境光なし
        }
    }
}