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
    // ���E�`�F�b�N�t���Ń}�e���A�����擾
    if (instanceID < 32) // �z�肳���ő�C���X�^���X��
    {
        return MaterialBuffer[instanceID];
    }
    else
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
    }
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
    // 1. �C���X�^���X�̃I�t�Z�b�g�����擾
    InstanceOffsetData offsetData = InstanceOffsetBuffer[instanceID];
    
    // 2. �v���~�e�B�uID����O�p�`��3�̃C���f�b�N�X���擾
    uint indexOffset = offsetData.indexOffset + primitiveID * 3;
    uint i0 = IndexBuffer[indexOffset + 0] + offsetData.vertexOffset;
    uint i1 = IndexBuffer[indexOffset + 1] + offsetData.vertexOffset;
    uint i2 = IndexBuffer[indexOffset + 2] + offsetData.vertexOffset;
    
    // 3. 3�̒��_�̖@�����擾�i���[�J�����W�j
    float3 n0 = VertexBuffer[i0].normal;
    float3 n1 = VertexBuffer[i1].normal;
    float3 n2 = VertexBuffer[i2].normal;
    
    // ������ �f�o�b�O�F���_�@�������̂܂ܕԂ��i�ϊ��Ȃ��j������
    float3 interpolatedNormal = n0 * (1.0f - barycentrics.x - barycentrics.y) +
                               n1 * barycentrics.x +
                               n2 * barycentrics.y;
    
    return normalize(interpolatedNormal);
}

// ������ ���[���h�ϊ��ł̖@���擾 ������
float3 GetWorldNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    // ���[�J���@�����擾
    float3 localNormal = GetInterpolatedNormal(instanceID, primitiveID, barycentrics);
    
    // ���[���h�ϊ���K�p
    float3x4 objectToWorld = ObjectToWorld3x4();
    
    // �@���ϊ��i��]�̂݁j
    float3 worldNormal = float3(
        dot(localNormal, objectToWorld._m00_m10_m20),
        dot(localNormal, objectToWorld._m01_m11_m21),
        dot(localNormal, objectToWorld._m02_m12_m22)
    );
    
    return normalize(worldNormal);
}
#endif // COMMON_HLSLI