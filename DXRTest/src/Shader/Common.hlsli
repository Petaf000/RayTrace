// shader/Common.hlsli - ���ʃw�b�_�[�i�f�o�b�O�Łj
#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#include "Utils.hlsli"
#include "GeometryData.hlsli"
#include "LightFunc.hlsli"

// �萔�o�b�t�@
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float4 lightPosition;
    float4 lightColor;
};

// �O���[�o�����\�[�X
RaytracingAccelerationStructure SceneBVH : register(t0);
RWTexture2D<float4> RenderTarget : register(u0);

// ������ �ǉ��F�e��o�b�t�@ ������
StructuredBuffer<MaterialData> MaterialBuffer : register(t1);
StructuredBuffer<DXRVertex> VertexBuffer : register(t2);
StructuredBuffer<uint> IndexBuffer : register(t3);
StructuredBuffer<InstanceOffsetData> InstanceOffsetBuffer : register(t4);

// ������ �ǉ��F�}�e���A���擾�w���p�[�֐� ������
MaterialData GetMaterial(uint instanceID)
{
    InstanceOffsetData instanceData = InstanceOffsetBuffer[instanceID];
    
    // TODO: CPP���Ń}�e���A�����Ȃ����̏�����ǉ�����
    /*if (instanceData.materialID == 0xFFFFFFFF)
    {
        // �f�t�H���g�}�e���A��
        MaterialData defaultMaterial;
        defaultMaterial.albedo = float3(1.0f, 0.0f, 1.0f); // �}�[���^�i�G���[�F�j
        defaultMaterial.roughness = 1.0f;
        defaultMaterial.refractiveIndex = 1.0f;
        defaultMaterial.emission = float3(0, 0, 0);
        defaultMaterial.materialType = MATERIAL_LAMBERTIAN;
        defaultMaterial.padding = 0.0f;
        return defaultMaterial;
    }*/
    return MaterialBuffer[instanceData.materialID];
}

// ������ �f�o�b�O�p�F�@����F�ɕϊ�����֐� ������
float3 NormalToColor(float3 normal)
{
    // �@���x�N�g�� (-1 to 1) ��F (0 to 1) �ɕϊ�
    return normal * 0.5f + 0.5f;
}
// ������ �ł��V���v���Ȗ@���擾�i�f�o�b�O�p�j������
float3 GetInterpolatedNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    // �I�t�Z�b�g�����擾
    InstanceOffsetData offset = InstanceOffsetBuffer[instanceID];
    
    // �O�p�`�̃C���f�b�N�X���擾  
    uint baseIndex = offset.indexOffset + primitiveID * 3;
    
    // ���_�C���f�b�N�X�擾
    uint i0 = IndexBuffer[baseIndex + 0] + offset.vertexOffset;
    uint i1 = IndexBuffer[baseIndex + 1] + offset.vertexOffset;
    uint i2 = IndexBuffer[baseIndex + 2] + offset.vertexOffset;
    
    // ���_�@�����d�S���W�ŕ��
    float3 normal = VertexBuffer[i0].normal * (1.0f - barycentrics.x - barycentrics.y) +
                    VertexBuffer[i1].normal * barycentrics.x +
                    VertexBuffer[i2].normal * barycentrics.y;
    
    return normalize(normal);
}

// ������ ���[���h�ϊ��ł̖@���擾�i�C���Łj ������
float3 GetWorldNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    // ���[�J���@�����擾
    float3 localNormal = GetInterpolatedNormal(instanceID, primitiveID, barycentrics);

    // ���[���h�ϊ��s����擾
    float3x4 objectToWorld = ObjectToWorld3x4();

    // �@�������[���h��Ԃɕϊ�
    // objectToWorld�̍����3x3�����i��]�ƃX�P�[�����O��S���j����Z����
    float3 worldNormal = mul((float3x3) objectToWorld, localNormal);

    // �ϊ���̖@���𐳋K�����ĕԂ�
    return normalize(worldNormal);
}
#endif // COMMON_HLSLI