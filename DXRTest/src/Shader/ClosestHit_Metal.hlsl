#include "Common.hlsli"

// �E�E�E Metal�}�e���A���pBRDF�T���v�����O �E�E�E
BRDFSample SampleMetalBRDF(float3 normal, float3 rayDir, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    // ���ʔ��˕���
    float3 reflected = reflect(rayDir, normal);
    
    // ���t�l�X�ɂ�郉���_������ǉ�
    float3 perturbation = material.roughness * RandomUnitVector(seed);
    sample.direction = normalize(reflected + perturbation);
    
    // �@���Ƃ̓��σ`�F�b�N
    float NdotL = dot(sample.direction, normal);
    if (NdotL > 0.0f)
    {
        sample.brdf = material.albedo; // ������BRDF
        sample.pdf = 1.0f; // �f���^�֐��I�i�X�y�L�����[�j
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
    }
    
    return sample;
}

[shader("closesthit")]
void ClosestHit_Metal(inout RayPayload payload, in VertexAttributes attr)
{
    // �ő唽�ˉ񐔃`�F�b�N
    if (payload.depth >= 6)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    uint instanceID = InstanceID();
    MaterialData material = GetMaterial(instanceID);
    
    // ��_���v�Z
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // ���m�Ȗ@���𒸓_�f�[�^����擾
    uint primitiveID = PrimitiveIndex();
    float3 normal = GetInterpolatedNormal(instanceID, primitiveID, attr.barycentrics);
    
    // ���C�����Ɩ@���̌������m�F
    float3 rayDir = normalize(WorldRayDirection());
    if (dot(normal, rayDir) > 0.0f)
    {
        normal = -normal;
    }
    
    // ��������
    float3 emitted = material.emission;
    
    // �E�E�E �X�y�L�����[�ގ��FCPU���C�g���Ɠ������� �E�E�E
    BRDFSample brdfSample = SampleMetalBRDF(normal, rayDir, material, payload.seed);
    
    if (brdfSample.valid)
    {
        // ���˃��C���g���[�X
        RayDesc ray;
        ray.Origin = OffsetRay(worldPos, normal);
        ray.Direction = brdfSample.direction;
        ray.TMin = 0.001f;
        ray.TMax = 1000.0f;
        
        RayPayload newPayload;
        newPayload.color = float3(0, 0, 0);
        newPayload.depth = payload.depth + 1;
        newPayload.seed = payload.seed;
        
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, newPayload);
        
        // CPU���C�g���Ɠ����Femitted + mulPerElem(albedo, color)
        payload.color = emitted + brdfSample.brdf * newPayload.color;
    }
    else
    {
        payload.color = emitted;
    }
}