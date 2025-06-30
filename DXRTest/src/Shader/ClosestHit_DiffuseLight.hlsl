// ===== ClosestHit_DiffuseLight.hlsl �̏C�� =====
#include "Common.hlsli"

[shader("closesthit")]
void ClosestHit_DiffuseLight(inout RayPayload payload, in VertexAttributes attr)
{
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    // ��_���v�Z
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // �@���`�F�b�N�i�\�ʂ���̃��C�̂ݔ����j
    float3 rayDir = normalize(WorldRayDirection());
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    // G-Buffer�f�[�^��ݒ�i�v���C�}�����C�̏ꍇ�̂݁j
    SetGBufferData(payload, worldPos, normal, material.emission,
                   MATERIAL_LIGHT, 0.0f, RayTCurrent());
    
    // �\�ʂ���̃��C�̂ݔ����i���ʂ͔������Ȃ��j
    if (dot(rayDir, normal) < 0.0f)
    {
        // �v���C�}�����C�̏ꍇ�͋�������
        if (payload.depth == 0)
        {
            payload.color = material.emission;
        }
        else
        {
            // �ԐڏƖ��ł̌����̊�^�i������߂�j
            payload.color = material.emission * 0.8f;
        }
    }
    else
    {
        // ���ʂ͔������Ȃ�
        payload.color = float3(0, 0, 0);
    }
}