#include "raytrace.hlsli"

[shader("raygeneration")]
void RaygenShader()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    
    float2 d = (((launchIndex.xy + 0.5f) / launchDim.xy) * 2.f - 1.f);
    d.y = -d.y; // Y���𔽓]
    
    // �J�����p�����[�^
    float3 origin = float3(0, 5, -10); // �J�����ʒu
    float3 lookAt = float3(0, 5, 0); // �����_ (�R�[�l���{�b�N�X�̒��S)
    float3 up = float3(0, 1, 0); // �����
    float fov = 60.0f * (3.14159f / 180.0f); // ����p�i���W�A���j
    
    // �J������Ԃ̍\�z
    float3 cameraZ = normalize(lookAt - origin); // �O����
    float3 cameraX = normalize(cross(up, cameraZ)); // �E����
    float3 cameraY = cross(cameraZ, cameraX); // ������̍Čv�Z
    
    // ��ʂ̃A�X�y�N�g��
    float aspectRatio = float(launchDim.x) / float(launchDim.y);
    
    // ���C�̕����v�Z
    float tanHalfFovy = tan(fov / 2.0f);
    float3 rayDir = normalize(
        cameraZ +
        cameraX * d.x * tanHalfFovy * aspectRatio +
        cameraY * d.y * tanHalfFovy
    );
    
    // �����V�[�h�̏�����
    float seed = float(launchIndex.x + launchIndex.y * launchDim.x) / float(launchDim.x * launchDim.y);
    seed = frac(seed * 1337.0f); // �����V�[�h
    
    // �T���v�����i�A���`�G�C���A�V���O�⃂���e�J�����p�j
    const uint samplesPerPixel = 4;
    float4 finalColor = float4(0, 0, 0, 0);
    
    for (uint sample = 0; sample < samplesPerPixel; sample++)
    {
        // ���C�̍쐬
        RayDesc ray;
        ray.Origin = origin;
        ray.Direction = rayDir;
        ray.TMin = 0.001;
        ray.TMax = 100.0;
        
        // �y�C���[�h������
        RayPayload payload;
        payload.color = float4(0, 0, 0, 0);
        payload.seed = seed + sample * 0.1f; // �T���v�����ƂɈقȂ�V�[�h
        payload.recursionDepth = 0;
        
        
        
        // ���C�𔭎�
        TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            ray,
            payload
        );
        
        // ���ʂ�~��
        finalColor += payload.color;
    }
    
    // ���ς����
    finalColor /= float(samplesPerPixel);
    
    // �g�[���}�b�s���O��K�p
    finalColor = float4(float3(1.0,1.0,1.0) - exp(-finalColor.rgb), 1.0f);
    
    // �K���}�␳
    finalColor.rgb = pow(finalColor.rgb, 1.0f / 2.2f);
    
    // ���ʂ��o��
    RenderTarget[launchIndex] = finalColor;
}