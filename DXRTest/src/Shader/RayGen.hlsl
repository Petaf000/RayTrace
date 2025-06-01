#include "Common.hlsli"

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    // �E�E�E ��葽���T���v�����Ńm�C�Y�팸 �E�E�E
    float3 finalColor = float3(0, 0, 0);
    const int SAMPLES = 25; // �T���v�����𑝂₷
    
    for (int sampleIndex = 0; sampleIndex < SAMPLES; sampleIndex++)
    {
        // �e�T���v���ňقȂ�V�[�h�l�𐶐�
        uint baseSeed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
        uint seed = WangHash(baseSeed + sampleIndex * 12345);
        
        // ��苭���W�b�^�[�i�t���s�N�Z���͈́j
        float2 jitter = float2(RandomFloat(seed), RandomFloat(seed)) - 0.5f;
        float2 crd = float2(launchIndex.xy) + jitter; // �t���W�b�^�[�K�p
        float2 dims = float2(launchDim.xy);
        
        // ���K�����W�ɕϊ� (-1 to 1)
        float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
        d.y = -d.y; // Y�����]
        
        // �E �J�����p�����[�^ �E
        float3 cameraPos = viewMatrix._m03_m13_m23;
        
        // �J�����̊��x�N�g���v�Z
        float3 forward = viewMatrix._m02_m12_m22;
        float3 right = normalize(viewMatrix._m00_m10_m20);
        float3 up = normalize(viewMatrix._m01_m11_m21);
        
        // FOV�v�Z
        float cotHalfFov = projectionMatrix._22;
        float tanHalfFov = 1.0f / cotHalfFov;
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
        
        // �E�E�E ���C�y�C���[�h������ �E�E�E
        RayPayload payload;
        payload.color = float3(0, 0, 0);
        payload.depth = 0;
        payload.seed = seed; // �T���v���ŗL�̃V�[�h
        
        // �E ���C�g���[�V���O���s �E
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
        
        // �T���v�����ʂ�~��
        finalColor += payload.color;
    }
    
    // ���ω�
    finalColor /= float(SAMPLES);
    
    // �E�E�E �|�X�g�v���Z�b�V���O �E�E�E
    float3 color = finalColor;
    
    // NaN�`�F�b�N
    if (any(isnan(color)) || any(isinf(color)))
    {
        color = float3(1, 0, 1); // �}�[���^�ŃG���[�\��
    }
    
    // ��莩�R�ȃg�[���}�b�s���O�i�L���̎�@���Q�l�j
    color = color / (0.8f + color); // �����T���߂�
    
    // �F�̍ʓx�������グ��
    float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    color = lerp(float3(luminance, luminance, luminance), color, 1.2f);
    
    // �K���}�␳
    color = pow(max(color, 0.0f), 1.0f / 2.2f);
    
    // ���ʂ��o��
    RenderTarget[launchIndex.xy] = float4(color, 1.0f);
}