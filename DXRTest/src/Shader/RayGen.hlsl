//=========================================
// shader/RayGen.hlsl - Ray Generation Shader (�C����)
#include "Common.hlsli"

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);
    
    // ���K�����W�ɕϊ� (-1 to 1)
    float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
    d.y = -d.y; // Y�����]
    
    // �� �J�����p�����[�^ ��
    float3 cameraPos = viewMatrix._m03_m13_m23;
    float3 cameraUp = float3(0.0f, 1.0f, 0.0f);
    
    // �J�����̊��x�N�g���v�Z
    float3 forward = viewMatrix._m02_m12_m22;
    float3 right = normalize(viewMatrix._m00_m10_m20);
    float3 up = normalize(viewMatrix._m01_m11_m21);
    
    // FOV
    float cotHalfFov = projectionMatrix._22;
    float tanHalfFov = 1.0f / cotHalfFov;
    float fov = 2.0f * atan(tanHalfFov);
    float aspect = dims.x / dims.y;
    
    // ���C�����v�Z
    float3 rayDir = normalize(forward +
                            d.x * right * tanHalfFov * aspect +
                            d.y * up * tanHalfFov);
    
    // ���C�ݒ�
    RayDesc ray;
    ray.Origin = cameraPos;
    ray.Direction = rayDir;
    ray.TMin = 0.1f;
    ray.TMax = 10000.0f;
    
    // ���C�y�C���[�h������
    RayPayload payload;
    payload.color = float3(0.2f, 0.4f, 0.8f); // �f�t�H���g�͐iMiss�j
    payload.depth = 0;
    payload.seed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
    
    // �� ���C�g���[�V���O���s ��
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
    
    // ���ʂ��o��
    RenderTarget[launchIndex.xy] = float4(payload.color, 1.0f);
    
    /*uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);
    
    // �X�N���[�����W�𐳋K�����W�ɕϊ� (-1 to 1)
    float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
    d.y = -d.y; // Y�����]
    
    // �� �C���F���������C���� ��
    RayDesc ray;
    ray.Origin = cameraPosition.xyz;
    
    // �r���[�v���W�F�N�V�����s��̋t�s����g�p���ă��[���h���W���v�Z
    // ���̕����͒萔�o�b�t�@�ɋt�s���ǉ����邩�A�V�F�[�_�[���Ōv�Z
    
    // �ȒP�ȕ��@�F�J�����p�����[�^���璼�ڌv�Z
    float3 cameraPos = cameraPosition.xyz;
    float3 cameraTarget = float3(278.0f, 278.0f, 278.0f); // �^�[�Q�b�g�ʒu�i�Œ�j
    float3 cameraUp = float3(0.0f, 1.0f, 0.0f);
    
    // �J�����̊��x�N�g���v�Z
    float3 forward = normalize(cameraTarget - cameraPos);
    float3 right = normalize(cross(forward, cameraUp));
    float3 up = cross(right, forward);
    
    // FOV�ƃA�X�y�N�g��i�萔�Ƃ��Đݒ�j
    float fov = radians(40.0f);
    float aspect = dims.x / dims.y;
    float tanHalfFov = tan(fov * 0.5f);
    
    // ���C�����v�Z
    float3 rayDir = normalize(forward +
                            d.x * right * tanHalfFov * aspect +
                            d.y * up * tanHalfFov);
    
    ray.Direction = rayDir;
    ray.TMin = 0.1f; // �����傫�߂̒l
    ray.TMax = 10000.0f;
    
    // ���C�y�C���[�h������
    RayPayload payload;
    payload.color = float3(1, 0, 1); // �f�o�b�O�p�}�[���^
    payload.depth = 0;
    payload.seed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
    
    // ���C�g���[�X���s
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
    
    // ���ʂ��o��
    RenderTarget[launchIndex.xy] = float4(payload.color, 1.0f);
*/
}