#include "Common.hlsli"

[shader("closesthit")]
void ClosestHit_DiffuseLight(inout RayPayload payload, in VertexAttributes attr)
{
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    // �E�E�E ���C�g�}�e���A���F�����̂� �E�E�E
    // ���C�g�͌��𔭂��邾���ŁA���̃��C�͔��ˁE���܂��Ȃ�
    
    // �@���`�F�b�N�i�\�ʂ���̃��C�̂ݔ����j
    float3 rayDir = normalize(WorldRayDirection());
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetInterpolatedNormal(instanceID, primitiveID, attr.barycentrics);
    
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