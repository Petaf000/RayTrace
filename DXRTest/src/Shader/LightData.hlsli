#ifndef LIGHTDATA_HLSLI
#define LIGHTDATA_HLSLI

// ������ ���C�g���\���� ������
struct LightInfo
{
    float3 position;
    float3 size;
    float3 emission;
    float area;
    uint instanceID;
    float3 normal;
    float2 padding;
};

// ������ PDF�\���̂ƃT���v�����O�֐� ������

struct LightSample
{
    float3 position;
    float3 direction;
    float3 radiance;
    float distance;
    float pdf;
    bool valid;
};

// Cornell Box���C�g���i�Œ�j
LightInfo GetLightInfo()
{
    LightInfo light;
    light.position = float3(0.0f, 267.5f, -227.0f);
    light.size = float3(130.0f, 5.0f, 105.0f);
    light.emission = float3(15.0f, 15.0f, 15.0f);
    light.area = light.size.x * light.size.z;
    light.instanceID = 5; // ���C�g�̃C���X�^���XID
    light.normal = float3(0, -1, 0); // ������
    return light;
}

struct BRDFSample
{
    float3 direction;
    float3 brdf;
    float pdf;
    bool valid;
};

#endif