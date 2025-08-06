#include "Common.hlsli"

[shader("miss")]
void Miss(inout RayPayload payload)
{
    // �V���h�E���C�̏ꍇ
    if (payload.depth == 999)
    {
        payload.color = float3(1, 1, 1); // �����͂�
        return;
    }
    
    // �E�E�E �����i�L�����Q�l�Ɏ��R�Ȑݒ�j �E�E�E
    
    // �v���C�}�����C�̏ꍇ�F��̐F
    if (payload.depth == 0)
    {
        // �Â������i�Q�l�R�[�h�ɍ��킹��j
        payload.color = float3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        // �ԐڏƖ��̏ꍇ�F����
        // 環境光なし：完全に黒色
        payload.color = float3(0.0f, 0.0f, 0.0f); // 黒：環境光なし
    }
}