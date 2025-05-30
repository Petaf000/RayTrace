//=========================================
// shader/ClosestHit_DiffuseLight.hlsl - �����}�e���A��
#include "Common.hlsli"
// �}�e���A���f�[�^�i���[�J�����[�g�V�O�l�`������j
ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);

[shader("closesthit")]
void ClosestHit_DiffuseLight(inout RayPayload payload, in VertexAttributes attr)
{
    // �����}�e���A���͌����Ȃ̂ŁAemission�F�𒼐ڕԂ�
    payload.color = MaterialCB.emission;
}