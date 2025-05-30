//=========================================
// shader/ClosestHit_Lambertian.hlsl - �ėp��
#include "Common.hlsli"
// �}�e���A���f�[�^�i���[�J�����[�g�V�O�l�`������j
ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);
[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    // �ő�[�x�`�F�b�N
    if (payload.depth >= 4)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    // �q�b�g�_�v�Z
    float3 hitPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // �v���~�e�B�u�C���f�b�N�X����ʂ𔻒�
    uint primitiveIndex = PrimitiveIndex();
    float3 normal;
    
    // �{�b�N�X�̖ʂɉ������@��
    uint faceIndex = primitiveIndex / 2; // 2�̎O�p�`��1�̖�
    
    switch (faceIndex)
    {
        case 0:
            normal = float3(0, 0, 1);
            break; // �O�� (Z+)
        case 1:
            normal = float3(0, 0, -1);
            break; // �w�� (Z-)
        case 2:
            normal = float3(1, 0, 0);
            break; // �E�� (X+)
        case 3:
            normal = float3(-1, 0, 0);
            break; // ���� (X-)
        case 4:
            normal = float3(0, 1, 0);
            break; // ��� (Y+)
        case 5:
            normal = float3(0, -1, 0);
            break; // ���� (Y-)
        default:
            normal = float3(0, 0, 1);
            break; // �f�t�H���g
    }
    
    // �I�u�W�F�N�g��Ԃ̖@�������[���h��Ԃɕϊ�
    float4x3 objectToWorld = ObjectToWorld4x3();
    normal = normalize(mul((float3x3) objectToWorld, normal));
    
    // ���C�����ʂɃq�b�g�����ꍇ�͖@���𔽓]
    if (dot(normal, -WorldRayDirection()) < 0.0f)
    {
        normal = -normal;
    }
    
    // �ʏ��Lambertian���ˏ���
    // �[�x0�ł������o�[�g���˂��s���i�{���̏����j
    // �ʏ��Lambertian���ˏ���
    // �����o�[�g����
    uint tempSeed = payload.seed;
    float3 direction = RandomUnitVector(tempSeed);
    payload.seed = tempSeed;
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
    
    // �F�̌v�Z - �}�e���A����albedo�ƎU���������̐F������
    float cosTheta = max(0.0f, dot(normal, scatterDirection));
    
    // ���ʂ̊�����ǉ��i0.1�{�j���ăR���g���X�g�����P
    float3 ambient = MaterialCB.albedo * 0.1f;
    payload.color = MaterialCB.albedo * scatteredPayload.color * cosTheta + ambient;
}