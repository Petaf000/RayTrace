#include "Common.hlsli"

// �ǉ���G-Buffer�o�͗p (������u0�ɉ�����)
RWTexture2D<float4> AlbedoOutput : register(u1);
RWTexture2D<float4> NormalOutput : register(u2);
RWTexture2D<float4> DepthOutput : register(u3);

[shader("raygeneration")]
void RayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    
    // �A���`�G�C���A�V���O�p�}���`�T���v�����O
    float3 finalColor = float3(0, 0, 0);
    float3 finalAlbedo = float3(0, 0, 0);
    float3 finalNormal = float3(0, 0, 0);
    float finalDepth = 0.0f;
    uint finalMaterialType = 0;
    float finalRoughness = 0.0f;
    
    const int SAMPLES = 10; // �T���v�����𒲐�
    
    for (int sampleIndex = 0; sampleIndex < SAMPLES; sampleIndex++)
    {
        // �e�T���v���ňقȂ�V�[�h�l�𐶐�
        uint baseSeed = WangHash(launchIndex.x + launchIndex.y * launchDim.x);
        uint seed = WangHash(baseSeed + sampleIndex * 12345);
        
        // ��苭���W�b�^�[�i�t���s�N�Z�����j
        float2 jitter = float2(RandomFloat(seed), RandomFloat(seed)) - 0.5f;
        float2 crd = float2(launchIndex.xy) + jitter; // �t���W�b�^�[�K�p
        float2 dims = float2(launchDim.xy);
        
        // ���K�����W�ɕϊ� (-1 to 1)
        float2 d = ((crd + 0.5f) / dims) * 2.0f - 1.0f;
        d.y = -d.y; // Y�����]
        
        // �J�����p�����[�^
        float3 cameraPos = viewMatrix._m03_m13_m23;
        
        // �J�����̌����x�N�g���v�Z
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
        
        // ���C�y�C���[�h�������i�S�t�B�[���h�𖾎��I�ɏ������j
        RayPayload payload;
        payload.color = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.depth = 0; // 4 bytes
        payload.seed = seed; // 4 bytes
        
        // G-Buffer�f�[�^�������i�S�t�B�[���h�𖾎��I�ɏ������j
        payload.albedo = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.normal = float3(0, 0, 1); // 4+4+4 = 12 bytes (�f�t�H���g�@��)
        payload.worldPos = float3(0, 0, 0); // 4+4+4 = 12 bytes
        payload.hitDistance = 0.0f; // 4 bytes
        payload.materialType = 0; // 4 bytes
        payload.roughness = 0.0f; // 4 bytes
        payload.padding = 0; // 4 bytes
        
        // ���v: 72 bytes
        
        // ���C�g���[�V���O���s
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
        
        // �T���v�����ʂ�~��
        finalColor += payload.color;
        
        // �v���C�}�����C��G-Buffer�f�[�^�̂ݒ~��
        if (sampleIndex == 0)  // ������ �ŏ��̃T���v���݂̂Ŕ�����ȑf�� ������
        {
            finalAlbedo += payload.albedo;
            finalNormal += payload.normal;
            finalDepth += payload.hitDistance;
            finalMaterialType = payload.materialType;
            finalRoughness += payload.roughness;
        }
    }
    
    // ���ω�
    finalColor /= float(SAMPLES);
    finalAlbedo /= float(SAMPLES);
    finalNormal /= float(SAMPLES);
    finalDepth /= float(SAMPLES);
    finalRoughness /= float(SAMPLES);
    
    // �@���̐��K��
    if (length(finalNormal) > 0.1f)
    {
        finalNormal = normalize(finalNormal);
    }
    else
    {
        finalNormal = float3(0, 0, 1); // �f�t�H���g�@��
    }
    
    // �|�X�g�v���Z�V���O
    float3 color = finalColor;
    
    // NaN�`�F�b�N
    if (any(isnan(color)) || any(isinf(color)))
    {
        color = float3(1, 0, 1); // �}�[���^�ŃG���[�\��
    }
    
    // ��莩�R�ȃg�[���}�b�s���O�i�L���̋Z�@���Q�l�j
    color = color / (0.8f + color); // �����T���߂�
    
    // �F�̍ʓx�������グ��
    float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    color = lerp(float3(luminance, luminance, luminance), color, 1.2f);
    
    // �K���}�␳
    color = pow(max(color, 0.0f), 1.0f / 2.2f);
    
    // G-Buffer�̃K���}�␳��NaN�`�F�b�N
    if (any(isnan(finalAlbedo)) || any(isinf(finalAlbedo)))
        finalAlbedo = float3(0, 0, 0);
    if (any(isnan(finalNormal)) || any(isinf(finalNormal)))
        finalNormal = float3(0, 0, 1); // �f�t�H���g�@��
    if (isnan(finalDepth) || isinf(finalDepth))
        finalDepth = 0.0f;
    
    // ������ �C���F�[�x�l�𐳋K�����ĉ��� ������
    const float MAX_VIEW_DEPTH = 1000.0f; // �J������Far�N���b�v�Ȃǂɍ��킹��
    float normalizedDepth = saturate(finalDepth / MAX_VIEW_DEPTH);

    // ���ʂ��o��
    RenderTarget[launchIndex.xy] = float4(color, 1.0f);
    AlbedoOutput[launchIndex.xy] = float4(finalAlbedo, finalRoughness);
    NormalOutput[launchIndex.xy] = float4(finalNormal, 1.0f);
    // ������ �C���F���K�����ꂽ�[�x���������� ������
    DepthOutput[launchIndex.xy] = float4(normalizedDepth, normalizedDepth, normalizedDepth, 1.0f);
}