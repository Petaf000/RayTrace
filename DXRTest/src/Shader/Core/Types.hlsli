#ifndef TYPES_HLSLI
#define TYPES_HLSLI

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

struct VertexAttributes
{
    float2 barycentrics : SV_IntersectionAttributes;
};

struct InstanceOffsetData
{
    uint vertexOffset;
    uint indexOffset;
    uint materialID;
    uint padding;
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

struct RayPayload
{
    float3 color;
    uint depth;
    uint seed;
    
    // G-Buffer用データ
    float3 albedo;
    float3 normal;
    float3 worldPos;
    float hitDistance;
    uint materialType;
    float roughness;
    uint padding;
};

struct LightInfo
{
    float3 position;
    float padding1;
    float3 emission;
    float padding2;
    float3 size;
    float area;
    float3 normal;
    uint lightType;
    uint instanceID;
    float3 lightPadding;
};

struct LightSample
{
    float3 position;
    float3 direction;
    float3 radiance;
    float distance;
    float pdf;
    bool valid;
};

struct BRDFSample
{
    float3 direction;
    float3 brdf;
    float pdf;
    bool valid;
};

struct LightReservoir
{
    uint lightIndex;
    float3 samplePos;
    float3 radiance;
    float weight;
    uint sampleCount;
    float weightSum;
    float pdf;
    bool valid;
    float padding;
};

struct PathReservoir
{
    float3 pathVertex;
    float3 pathDirection;
    float3 pathRadiance;
    float pathLength;
    float weight;
    uint bounceCount;
    float pdf;
    bool valid;
};

struct PathVertex
{
    float3 position;
    float3 normal;
    float3 albedo;
    float3 emission;
    uint materialType;
    float roughness;
    float2 padding;
};

struct GIPath
{
    PathVertex vertices[4];
    float3 throughput;
    float3 radiance;
    uint vertexCount;
    float pathPdf;
    float misWeight;
    bool valid;
};

struct GIReservoir
{
    GIPath selectedPath;
    float weight;
    uint sampleCount;
    float weightSum;
    float pathPdf;
    bool valid;
    float3 padding;
};

#endif