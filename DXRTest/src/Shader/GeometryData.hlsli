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

// �g�����ꂽ���C�y�C���[�h
struct RayPayload
{
    float3 color;
    uint depth;
    uint seed;
    
    // G-Buffer�p�f�[�^ (�v���C�}�����C�̏ꍇ�̂ݎg�p)
    float3 albedo; // �}�e���A���̃A���x�h
    float3 normal; // ���[���h��Ԗ@��
    float3 worldPos; // ���[���h���W
    float hitDistance; // �q�b�g�����i�[�x�p�j
    uint materialType; // �}�e���A���^�C�v
    float roughness; // ���t�l�X�l
    uint padding; // �A���C�����g�p
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
    uint materialID;
    uint padding;
};

// G-Buffer�f�[�^��ݒ肷��w���p�[�֐�
void SetGBufferData(inout RayPayload payload, float3 worldPos, float3 worldNormal,
                   float3 albedo, uint materialType, float roughness, float hitDistance)
{
    // �v���C�}�����C�̏ꍇ�̂�G-Buffer�f�[�^��ݒ�
    if (payload.depth == 0)
    {
        payload.worldPos = worldPos;
        payload.normal = worldNormal;
        payload.albedo = albedo;
        payload.materialType = materialType;
        payload.roughness = roughness;
        payload.hitDistance = hitDistance;
    }
}

#endif