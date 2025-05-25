#include "raytrace.hlsli"

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    // �ċA�̐[���`�F�b�N - �����̍ċA�I�ȃo�E���X�𐧌�
    if (payload.recursionDepth >= 5)
    {
        payload.color = float4(0, 0, 0, 1);
        return;
    }
    
    // �I�u�W�F�N�g�ƃv���~�e�B�u�̎��ʏ����擾
    uint instanceID = InstanceID();
    uint primitiveIndex = PrimitiveIndex();
    
    // �o���Z���g���b�N���W�̌v�Z
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    
    // �O�p�`�̒��_�C���f�b�N�X���v�Z
    uint indexBase = 3 * primitiveIndex;
    
    // �O�p�`�̒��_���擾
    float3 v0 = Vertices[indexBase];
    float3 v1 = Vertices[indexBase + 1];
    float3 v2 = Vertices[indexBase + 2];
    
    // �@�����v�Z
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 triangleNormal = normalize(cross(edge1, edge2));
    
    // �I�u�W�F�N�g��Ԃ��烏�[���h��Ԃւ̕ϊ�
    float4x3 objectToWorld = ObjectToWorld4x3();
    float3 worldNormal = normalize(mul(triangleNormal, (float3x3) objectToWorld));
    
    // �����ʒu�̌v�Z
    float3 hitPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    
    // �I�u�W�F�N�g�̃}�e���A�����擾
    MaterialData material = Materials[instanceID];
    
    // �}�e���A���^�C�v�Ɋ�Â�����
    // 1. �����}�e���A�� - ���ڐF��Ԃ�
    if (length(material.emissiveColor.rgb) > 0.0f)
    {
        payload.color = material.emissiveColor;
        return;
    }
    
    // 2. �}�e���A���ʂ̏���
    if (material.metallic > 0.5f) // ���^�� (���ʔ���)
    {
        // ���˕������v�Z
        float3 reflectDir = reflect(WorldRayDirection(), worldNormal);
        
        // ���t�l�X�Ɋ�Â������_����
        reflectDir = RandomReflectionDirection(reflectDir, worldNormal, material.roughness, payload.seed);
        
        // ���˃��C��ݒ�
        RayDesc reflectionRay;
        reflectionRay.Origin = hitPosition + worldNormal * 0.001f; // ���Ȍ���������邽�߂̃I�t�Z�b�g
        reflectionRay.Direction = reflectDir;
        reflectionRay.TMin = 0.001f;
        reflectionRay.TMax = 100.0f;
        
        // �V�����y�C���[�h������
        RayPayload reflectionPayload;
        reflectionPayload.color = float4(0, 0, 0, 0);
        reflectionPayload.seed = payload.seed;
        reflectionPayload.recursionDepth = payload.recursionDepth + 1;
        
        // ���˃��C�𔭎�
        TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            reflectionRay,
            reflectionPayload
        );
        
        // �����̐F��K�p
        float3 reflectionColor = reflectionPayload.color.rgb * material.baseColor.rgb;
        payload.color = float4(reflectionColor, 1.0f);
    }
    else if (material.ior > 1.0f) // �U�d�� (����/����)
    {
        // ���ܗ��Ɠ��ˊp���v�Z
        float cosine = dot(-WorldRayDirection(), worldNormal);
        float etai_over_etat = (cosine > 0.0f) ? (1.0f / material.ior) : material.ior;
        float3 outwardNormal = (cosine > 0.0f) ? worldNormal : -worldNormal;
        
        // ���˕����Ƌ��ܕ������v�Z
        float3 reflectDir = reflect(WorldRayDirection(), worldNormal);
        float3 refractDir;
        
        // ����/���܂̌���
        float reflectProb;
        if (Refract(WorldRayDirection(), outwardNormal, etai_over_etat, refractDir))
        {
            // �t���l�������v�Z
            reflectProb = SchlickFresnel(cosine, material.ior);
        }
        else
        {
            // �S����
            reflectProb = 1.0f;
        }
        
        // �m���Ɋ�Â��Ĕ��˂܂��͋��܂�I��
        float3 rayDir;
        float3 origin;
        
        if (Random(payload.seed) < reflectProb)
        {
            // ����
            rayDir = reflectDir;
            origin = hitPosition + worldNormal * 0.001f;
        }
        else
        {
            // ����
            rayDir = refractDir;
            origin = hitPosition - worldNormal * 0.001f;
        }
        
        // ���̃��C��ݒ�
        RayDesc nextRay;
        nextRay.Origin = origin;
        nextRay.Direction = rayDir;
        nextRay.TMin = 0.001f;
        nextRay.TMax = 100.0f;
        
        // �V�����y�C���[�h������
        RayPayload nextPayload;
        nextPayload.color = float4(0, 0, 0, 0);
        nextPayload.seed = payload.seed;
        nextPayload.recursionDepth = payload.recursionDepth + 1;
        
        // ���C�𔭎�
        TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            nextRay,
            nextPayload
        );
        
        // �F�𓧖��ގ��̐F�Ńt�B���^�����O
        float3 finalColor = nextPayload.color.rgb * material.baseColor.rgb;
        payload.color = float4(finalColor, 1.0f);
    }
    else // �����o�[�g (�g�U����)
    {
        // �g�U���˕����𐶐�
        float3 diffuseDir = RandomHemisphereDirection(worldNormal, payload.seed);
        
        // �g�U���˃��C��ݒ�
        RayDesc diffuseRay;
        diffuseRay.Origin = hitPosition + worldNormal * 0.001f; // ���Ȍ���������邽�߂̃I�t�Z�b�g
        diffuseRay.Direction = diffuseDir;
        diffuseRay.TMin = 0.001f;
        diffuseRay.TMax = 100.0f;
        
        // �V�����y�C���[�h������
        RayPayload diffusePayload;
        diffusePayload.color = float4(0, 0, 0, 0);
        diffusePayload.seed = payload.seed;
        diffusePayload.recursionDepth = payload.recursionDepth + 1;
        
        // �g�U���˃��C�𔭎�
        TraceRay(
            SceneBVH,
            RAY_FLAG_NONE,
            0xFF,
            0,
            1,
            0,
            diffuseRay,
            diffusePayload
        );
        
        // �g�U���˂̌��ʂ�K�p
        float NdotL = max(0.0f, dot(worldNormal, diffuseDir));
        float3 diffuseColor = diffusePayload.color.rgb * material.baseColor.rgb * NdotL;
        
        payload.color = float4(diffuseColor, 1.0f);
    }
}