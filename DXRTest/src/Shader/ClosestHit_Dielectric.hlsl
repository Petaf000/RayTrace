// ===== ClosestHit_Dielectric.hlsl �̏C�� =====
#include "Common.hlsli"

// Schlick�ߎ��iDielectric�p�j
float Schlick(float cosine, float refractiveIndex)
{
    float r0 = (1.0f - refractiveIndex) / (1.0f + refractiveIndex);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

// Dielectric��p�T���v�����O
BRDFSample SampleDielectricBRDF(float3 normal, float3 rayDir, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    float3 outwardNormal;
    float niOverNt;
    float cosine;
    
    // ��������O���ւ̔���
    if (dot(rayDir, normal) > 0)
    {
        // ���C���������痈�Ă���
        outwardNormal = -normal;
        niOverNt = material.refractiveIndex;
        cosine = material.refractiveIndex * dot(rayDir, normal);
    }
    else
    {
        // ���C���O�����痈�Ă���
        outwardNormal = normal;
        niOverNt = 1.0f / material.refractiveIndex;
        cosine = -dot(rayDir, normal);
    }
    
    float3 refracted;
    float reflectProb;
    
    // ���܌v�Z
    float discriminant = 1.0f - niOverNt * niOverNt * (1.0f - cosine * cosine);
    if (discriminant > 0)
    {
        refracted = niOverNt * (rayDir - outwardNormal * cosine) - outwardNormal * sqrt(discriminant);
        reflectProb = Schlick(cosine, material.refractiveIndex);
    }
    else
    {
        // �S����
        reflectProb = 1.0f;
    }
    
    // ���˂����܂����m���I�Ɍ���
    if (RandomFloat(seed) < reflectProb)
    {
        // ����
        sample.direction = reflect(rayDir, outwardNormal);
    }
    else
    {
        // ����
        sample.direction = normalize(refracted);
    }
    
    sample.brdf = material.albedo; // �ʏ�͔��i�����j
    sample.pdf = 1.0f; // �X�y�L�����[
    sample.valid = true;
    
    return sample;
}

[shader("closesthit")]
void ClosestHit_Dielectric(inout RayPayload payload, in VertexAttributes attr)
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
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    float3 rayDir = normalize(WorldRayDirection());
    
    // G-Buffer�f�[�^��ݒ�i�v���C�}�����C�̏ꍇ�̂݁j
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_DIELECTRIC, material.roughness, RayTCurrent());
    
    // �K���X�F���܂Ɣ��˂̊m���I�I��
    // �K���X�͊�{�I�ɃX�y�L�����[�Ȃ̂ŁA���ڏƖ��̌v�Z�͕s�v
    
    BRDFSample brdfSample = SampleDielectricBRDF(normal, rayDir, material, payload.seed);
    
    if (brdfSample.valid)
    {
        // ���܂܂��͔��˃��C���g���[�X
        RayDesc ray;
        ray.Origin = OffsetRay(worldPos, normal);
        ray.Direction = brdfSample.direction;
        ray.TMin = 0.001f;
        ray.TMax = 1000.0f;
        
        RayPayload newPayload;
        newPayload.color = float3(0, 0, 0);
        newPayload.depth = payload.depth + 1;
        newPayload.seed = payload.seed;
        
        // G-Buffer�f�[�^���������i�Z�J���_�����C�p�j
        newPayload.albedo = float3(0, 0, 0);
        newPayload.normal = float3(0, 0, 1);
        newPayload.worldPos = float3(0, 0, 0);
        newPayload.hitDistance = 0.0f;
        newPayload.materialType = 0;
        newPayload.roughness = 0.0f;
        newPayload.padding = 0;
        
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, newPayload);
        
        // �K���X�F�Œ����i�ʏ�͓����Ȃ̂łقډe�����Ȃ��j
        payload.color = brdfSample.brdf * newPayload.color;
    }
    else
    {
        payload.color = float3(0, 0, 0);
    }
}