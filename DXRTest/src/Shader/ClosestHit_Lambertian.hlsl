//=========================================
// shader/ClosestHit_Lambertian.hlsl - Lambertian Material
#include "Common.hlsli"

// �}�e���A���f�[�^�i���[�J�����[�g�V�O�l�`������j
ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);

[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    // �ő�[�x�`�F�b�N
    if (payload.depth >= 3)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    // �q�b�g�_�Ɩ@���v�Z
    float3 hitPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // TODO: ���m�Ȗ@���v�Z�i���_�f�[�^�����ԁj
    // ���݂͊ȈՓI�Ƀ��C�����̋t���g�p
    float3 normal = -normalize(WorldRayDirection());
    
    // �����o�[�g����
    uint tempSeed = payload.seed;
    float3 direction = RandomUnitVector(tempSeed);
    payload.seed = tempSeed; // �X�V
    float3 scatterDirection = normalize(normal + direction);
    
    // �V�������C�̐���
    RayDesc scatteredRay;
    scatteredRay.Origin = OffsetRay(hitPoint, normal);
    scatteredRay.Direction = scatterDirection;
    scatteredRay.TMin = 0.001f;
    scatteredRay.TMax = 10000.0f;
    
    // �ċA�I���C�g���[�X
    RayPayload scatteredPayload;
    scatteredPayload.color = float3(0, 0, 0);
    scatteredPayload.depth = payload.depth + 1;
    scatteredPayload.seed = payload.seed;
    
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, scatteredRay, scatteredPayload);
    
    // �F�̌v�Z
    payload.color = MaterialCB.albedo * scatteredPayload.color * dot(normal, scatterDirection);
}