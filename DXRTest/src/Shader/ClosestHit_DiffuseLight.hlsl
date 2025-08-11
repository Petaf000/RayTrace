// ===== ClosestHit_DiffuseLight.hlsl の修正 =====
#include "Common.hlsli"

[shader("closesthit")]
void ClosestHit_DiffuseLight(inout RayPayload payload, in VertexAttributes attr)
{
    // デバッグモード：DiffuseLightシェーダー動作確認
    bool lightDebugMode = false;
    if (lightDebugMode && payload.depth == 0) {
        payload.color = float3(1.0f, 0.5f, 0.0f); // オレンジ色：DiffuseLightシェーダー実行
        return;
    }
    // セカンダリレイでも通常処理を行う（簡略化を除去）
    
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    // 頂点位置計算
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // 法線チェック（表面からのレイのみ発射）
    float3 rayDir = normalize(WorldRayDirection());
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    // G-Bufferデータを設定（プライマリレイの場合のみ）
    SetGBufferData(payload, worldPos, normal, material.emission,
                   MATERIAL_LIGHT, 0.0f, RayTCurrent());
    
    // 表面からのレイのみ発射（裏面は発射しない）
    if (dot(rayDir, normal) < 0.0f)
    {
        // プライマリレイの場合は発光する
        if (payload.depth == 0)
        {
            payload.color = material.emission;
        }
        else
        {
            // 間接照明での光源寄与（元の処理に戻す）
            payload.color = material.emission;
        }
    }
    else
    {
        // 裏面は発射しない
        payload.color = float3(0, 0, 0);
    }
}