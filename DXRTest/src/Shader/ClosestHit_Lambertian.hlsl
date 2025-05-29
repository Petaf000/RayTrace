//=========================================
// shader/ClosestHit_Lambertian.hlsl - �ŏI�Łi�}�e���A���F�\���j
#include "Common.hlsli"
// �}�e���A���f�[�^�i���[�J�����[�g�V�O�l�`������j
ConstantBuffer<MaterialData> MaterialCB : register(b1, space1);
[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    if (payload.depth == 0)
    {
        // �������I�ɐԐF��ݒ�
        payload.color = float3(0.65f, 0.05f, 0.05f); // CornelBoxScene�Őݒ肵���ԐF
        return;
    }
    
    payload.color = float3(0, 0, 0);
}