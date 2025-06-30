// ===== ClosestHit_Lambertian.hlsl �̏C�� =====
#include "Common.hlsli"

// LAMBERTIAN�g�UBRDF �T���v�����O
BRDFSample SampleLambertianBRDF(float3 normal, MaterialData material, inout uint seed)
{
    BRDFSample sample;
    
    // �R�T�C���d�ݕt���T���v�����O
    float r1 = RandomFloat(seed);
    float r2 = RandomFloat(seed);
    
    float cosTheta = sqrt(1.0f - r2);
    float sinTheta = sqrt(r2);
    float phi = 2.0f * PI * r1;
    
    // �Ǐ����W�n�ł̕���
    float3 localDir = float3(
        sinTheta * cos(phi),
        sinTheta * sin(phi),
        cosTheta
    );
    
    // �@����̍��W�n�ɕϊ�
    float3 up = abs(normal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    sample.direction = localDir.x * tangent + localDir.y * bitangent + localDir.z * normal;
    sample.brdf = material.albedo / PI; // Lambertian BRDF
    sample.pdf = cosTheta / PI; // �R�T�C���d�ݕt��PDF
    sample.valid = true;
    
    return sample;
}

[shader("closesthit")]
void ClosestHit_Lambertian(inout RayPayload payload, in VertexAttributes attr)
{
    // �ő唽�ˉ񐔃`�F�b�N
    if (payload.depth >= 6)
    {
        payload.color = float3(0, 0, 0);
        return;
    }
    
    uint instanceID = InstanceID();
    uint primitiveID = PrimitiveIndex();
    MaterialData material = GetMaterial(instanceID);
    
    // ��_���v�Z
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    
    // ���m�Ȗ@�������[���h��ԂŎ擾
    float3 normal = GetWorldNormal(instanceID, primitiveID, attr.barycentrics);
    
    // ���C�����Ɩ@���̌������m�F
    float3 rayDir = normalize(WorldRayDirection());
    if (dot(normal, rayDir) > 0.0f)
    {
        normal = -normal;
    }
    
    SetGBufferData(payload, worldPos, normal, material.albedo,
                   MATERIAL_LAMBERTIAN, material.roughness, RayTCurrent());
    
    // ��������
    float3 finalColor = 0.0f;
    
    // 1. ���ڏƖ��iNext Event Estimation�j
    LightSample lightSample = SampleAreaLight(worldPos, payload.seed);
    
    if (lightSample.valid)
    {
        float NdotL = max(0.0f, dot(normal, lightSample.direction));
        
        if (NdotL > 0.0f)
        {
            // �V���h�E���C�ŉ����`�F�b�N
            RayDesc shadowRay;
            shadowRay.Origin = OffsetRay(worldPos, normal);
            shadowRay.Direction = lightSample.direction;
            shadowRay.TMin = 0.001f;
            shadowRay.TMax = lightSample.distance - 0.001f;
            
            RayPayload shadowPayload;
            shadowPayload.color = float3(1, 1, 1);
            shadowPayload.depth = 999;
            shadowPayload.seed = payload.seed;
            
            TraceRay(SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                     0xFF, 0, 1, 0, shadowRay, shadowPayload);
            
            if (length(shadowPayload.color) > 0.5f)
            {
                // ���ڏƖ��̊�^
                float3 brdf = material.albedo / PI;
                finalColor += brdf * lightSample.radiance * NdotL / lightSample.pdf;
            }
        }
    }
    
    // 2. �ԐڏƖ��i�ȗ��� - �ŏ��̐���̂݁j
    if (payload.depth < 3) // �v�Z�R�X�g��}���邽�ߍŏ��̐���̂�
    {
        BRDFSample brdfSample = SampleLambertianBRDF(normal, material, payload.seed);
        
        if (brdfSample.valid)
        {
            // �ԐڏƖ����C���g���[�X
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
            
            // �ԐڏƖ��̊�^�i�K�x�ȏd�݁j
            finalColor += material.albedo * newPayload.color * 0.5f;
        }
    }
    
    payload.color = finalColor;
}