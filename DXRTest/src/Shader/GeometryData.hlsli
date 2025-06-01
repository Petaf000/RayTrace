#ifndef GEOMETRYDATA_HLSLI
#define GEOMETRYDATA_HLSLI

// �}�e���A���^�C�v
#define MATERIAL_LAMBERTIAN  0
#define MATERIAL_METAL       1
#define MATERIAL_DIELECTRIC  2
#define MATERIAL_LIGHT       3


// ���_�f�[�^�\����
struct DXRVertex
{
    float3 position;
    float3 normal;
    float2 texCoord;
};

// ���C�y�C���[�h
struct RayPayload
{
    float3 color;
    uint depth;
    uint seed;
};

// ���_����
struct VertexAttributes
{
    float2 barycentrics : SV_IntersectionAttributes;
};

// �}�e���A���f�[�^�i�V�F�[�_�[�p�j
struct MaterialData
{
    float3 albedo;
    float roughness;
    float refractiveIndex;
    float3 emission;
    int materialType;
    float padding;
};

// �C���X�^���X�I�t�Z�b�g���
struct InstanceOffsetData
{
    uint vertexOffset;
    uint indexOffset;
    uint2 padding;
};

#endif