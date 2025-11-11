#ifndef GEOMETRYDATA_HLSLI
#define GEOMETRYDATA_HLSLI

#define MATERIAL_LAMBERTIAN  0
#define MATERIAL_METAL       1
#define MATERIAL_DIELECTRIC  2
#define MATERIAL_LIGHT       3

struct DXRVertex
{
    float3 position;
    float3 normal;
    float2 texCoord;
};

struct RayPayload
{
    float3 color;
    uint depth;
    uint seed;
    
    float3 albedo;    float3 normal;    float3 worldPos;    float hitDistance;    uint materialType;    float roughness;    uint padding;};

struct VertexAttributes
{
    float2 barycentrics : SV_IntersectionAttributes;
};

struct MaterialData
{
    float3 albedo;
    float roughness;
    float refractiveIndex;
    float3 emission;
    int materialType;
    float padding;
};

struct InstanceOffsetData
{
    uint vertexOffset;
    uint indexOffset;
    uint materialID;
    uint padding;
};

void SetGBufferData(inout RayPayload payload, float3 worldPos, float3 worldNormal,
                   float3 albedo, uint materialType, float roughness, float hitDistance)
{
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