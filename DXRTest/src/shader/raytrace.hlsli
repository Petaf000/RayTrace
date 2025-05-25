// ���C�g���[�X�t���O�̒�`
#define RAY_FLAG_NONE 0x00
#define RAY_FLAG_FORCE_OPAQUE 0x01
#define RAY_FLAG_FORCE_NON_OPAQUE 0x02
#define RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH 0x04
#define RAY_FLAG_SKIP_CLOSEST_HIT_SHADER 0x08
#define RAY_FLAG_CULL_BACK_FACING_TRIANGLES 0x10
#define RAY_FLAG_CULL_FRONT_FACING_TRIANGLES 0x20
#define RAY_FLAG_CULL_OPAQUE 0x40
#define RAY_FLAG_CULL_NON_OPAQUE 0x80
#define RAY_FLAG_SKIP_TRIANGLES 0x100
#define RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES 0x200

// �}�e���A���^�C�v��`
#define MATERIAL_LAMBERT 0
#define MATERIAL_METAL 1
#define MATERIAL_DIELECTRIC 2
#define MATERIAL_EMISSIVE 3

// ���C�g���[�V���O�f�[�^�\��
struct RayPayload
{
    float4 color;
    float seed; // ���������p�V�[�h
    uint recursionDepth;
};

// �}�e���A���f�[�^�\��
struct MaterialData
{
    float4 baseColor; // ��{�F
    float4 emissiveColor; // �����F
    float metallic; // �����x (0-1)
    float roughness; // �e�� (0-1)
    float ior; // ���ܗ�
    float padding;
};

// ���_�\���̒�`�iCPU�Ƌ��L�j
struct Vertex
{
    float3 position;
    float4 color;
};

// ���\�[�X�o�C���f�B���O
RaytracingAccelerationStructure SceneBVH : register(t0);
RWTexture2D<float4> RenderTarget : register(u0);
StructuredBuffer<float3> Vertices : register(t1);
StructuredBuffer<float4> Colors : register(t2);
StructuredBuffer<MaterialData> Materials : register(t3);

// ���������֐�
float Random(inout float seed)
{
    seed = frac(sin(seed) * 43758.5453123);
    return seed;
}

// �������̃����_���ȕ����𐶐�
float3 RandomHemisphereDirection(float3 normal, inout float seed)
{
    float u1 = Random(seed);
    float u2 = Random(seed);
    
    float r = sqrt(u1);
    float theta = 2.0f * 3.1415926f * u2;
    
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(1.0f - u1);
    
    // �@������Ƃ������W�n���\�z
    float3 up = abs(normal.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    // ���[�J�����W���烏�[���h���W�ɕϊ�
    return tangent * x + bitangent * y + normal * z;
}

// ���˃x�N�g���Ƀ��t�l�X��K�p
float3 RandomReflectionDirection(float3 reflected, float3 normal, float roughness, inout float seed)
{
    if (roughness <= 0.0f)
        return reflected;
    
    float3 randomDir = RandomHemisphereDirection(normal, seed);
    return normalize(lerp(reflected, randomDir, roughness));
}

// �t���l�������� (�V�����b�N�̋ߎ�)
float SchlickFresnel(float cosine, float refractiveIndex)
{
    float r0 = (1.0f - refractiveIndex) / (1.0f + refractiveIndex);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

// ���܃x�N�g���v�Z (�X�l���̖@��)
bool Refract(float3 v, float3 n, float niOverNt, out float3 refracted)
{
    float dt = dot(v, n);
    float discriminant = 1.0f - niOverNt * niOverNt * (1.0f - dt * dt);
    if (discriminant > 0.0f)
    {
        refracted = niOverNt * (v - n * dt) - n * sqrt(discriminant);
        return true;
    }
    return false;
}