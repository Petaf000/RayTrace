#ifndef UTIL_HLSLI
#define UTIL_HLSLI

// ���w�I�萔
#define PI 3.14159265359f
#define INV_PI 0.31830988618f

// ���[�e�B���e�B�֐�
float3 OffsetRay(const float3 p, const float3 n)
{
    return p + n * 0.001f; // 元のオフセットに戻す
}

// ���������i�ȈՔŁj
uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

// **強化されたハッシュ関数（PCG-based）**
uint PCGHash(uint seed)
{
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// **XorShift ハッシュ関数（高速・高品質）**
uint XorShiftHash(uint seed)
{
    seed ^= seed << 13u;
    seed ^= seed >> 17u;
    seed ^= seed << 5u;
    return seed;
}

// **Multi-Hash関数（複数アルゴリズム組み合わせ）**
uint MultiHash(uint seed)
{
    // 3つのハッシュ関数を組み合わせて最高品質を実現
    seed = WangHash(seed);
    seed = PCGHash(seed);
    seed = XorShiftHash(seed);
    return seed;
}

float RandomFloat(inout uint seed)
{
    seed = PCGHash(seed); // より高品質なハッシュ関数を使用
    return float(seed) / 4294967295.0f; // 32bit全体を使用して精度向上
}

float3 RandomUnitVector(inout uint seed)
{
    float z = RandomFloat(seed) * 2.0f - 1.0f;
    float a = RandomFloat(seed) * 2.0f * PI;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

float3 RandomCosineDirection(inout uint seed)
{
    float r1 = RandomFloat(seed);
    float r2 = RandomFloat(seed);
    
    float z = sqrt(1.0f - r2);
    float phi = 2.0f * PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    
    return float3(x, y, z);
}

#endif