// ===== ClosestHit_Dielectric.hlsl ���S���������� =====
#include "Common.hlsli"

// CPU�ł�Util.h�Ɗ��S�ɓ���Schlick�ߎ�
float schlick(float cosine, float ri)
{
    float r0 = (1.0f - ri) / (1.0f + ri);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

// CPU�ł�Util.h�Ɗ��S�ɓ���reflect�֐�
float3 reflect_vec(float3 v, float3 n)
{
    return v - 2.0f * dot(v, n) * n;
}

// CPU�ł�Util.h�Ɗ��S�ɓ���refract�֐�
bool refract_vec(float3 v, float3 n, float ni_over_nt, out float3 refracted)
{
    float3 uv = normalize(v);
    float dt = dot(uv, n);
    float D = 1.0f - (ni_over_nt * ni_over_nt) * (1.0f - dt * dt);
    if (D > 0.0f)
    {
        refracted = -ni_over_nt * (uv - n * dt) - n * sqrt(D);
        return true;
    }
    else
    {
        return false;
    }
}

// CPU�ł�Dielectric::scatter�Ɗ��S�ɓ������W�b�N
bool DielectricScatter(float3 rayDir, float3 normal, float refractiveIndex, inout uint seed, out float3 scatteredDir, out float3 attenuation)
{
    float3 outward_normal;
    float3 reflected = reflect_vec(rayDir, normal);
    float ni_over_nt;
    float reflect_prob;
    float cosine;
    
    // CPU�łƊ��S�ɓ�������
    if (dot(rayDir, normal) > 0.0f)
    {
        outward_normal = -normal;
        ni_over_nt = refractiveIndex;
        cosine = refractiveIndex * dot(rayDir, normal) / length(rayDir);
    }
    else
    {
        outward_normal = normal;
        ni_over_nt = 1.0f / refractiveIndex;
        cosine = -dot(rayDir, normal) / length(rayDir);
    }
    
    // CPU�łƓ����A���x�h�ݒ�
    attenuation = float3(1.0f, 1.0f, 1.0f);
    
    float3 refracted;
    if (refract_vec(-rayDir, outward_normal, ni_over_nt, refracted))
    {
        reflect_prob = schlick(cosine, refractiveIndex);
    }
    else
    {
        reflect_prob = 1.0f;
    }
    
    // CPU�ł�drand48()�Ɠ����m������
    if (RandomFloat(seed) < reflect_prob)
    {
        scatteredDir = reflected;
    }
    else
    {
        scatteredDir = refracted;
    }
    
    return true;
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
    
    // CPU�łƓ�����WorldRayDirection()�����̂܂܎g�p�i���K�����Ȃ��j
    float3 rayDir = WorldRayDirection();
    
    // G-Buffer�f�[�^��ݒ�
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   2, material.roughness, RayTCurrent()); // 2 = MATERIAL_DIELECTRIC
    
    // CPU�łƊ��S�ɓ���Dielectric�̎U���v�Z
    float3 scatteredDir;
    float3 attenuation;
    
    if (DielectricScatter(rayDir, normal, material.refractiveIndex, payload.seed, scatteredDir, attenuation))
    {
        // �U�����C���g���[�X
        RayDesc ray;
        ray.Origin = worldPos; // CPU�łƓ������P���ȊJ�n�_
        ray.Direction = normalize(scatteredDir); // �����𐳋K��
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
        
        // CPU�łƓ������w�ʃJ�����O�Ȃ�
        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, newPayload);
        
        // CPU�łƓ������Aattenuation�i��ɔ��j���|����
        payload.color = attenuation * newPayload.color;
    }
    else
    {
        payload.color = float3(0, 0, 0);
    }
}