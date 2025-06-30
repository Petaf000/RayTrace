#include "Common.hlsli"

// A-Trous Wavelet Denoiser Compute Shader
// Using global root signature - ���W�X�^�͊�����G-Buffer�p���g�p

// G-Buffer: t5(Albedo), t6(Normal), t7(Depth) ���g�p
Texture2D<float4> AlbedoBuffer : register(t5); // G-Buffer�̃A���x�h  
Texture2D<float4> NormalBuffer : register(t6); // G-Buffer�̖@��
Texture2D<float4> DepthBuffer : register(t7); // G-Buffer�̐[�x
RWTexture2D<float4> DenoiserOutput : register(u4); // �f�m�C�U�[�o��

// �f�m�C�U�[�p�萔�o�b�t�@
cbuffer DenoiserConstants : register(b1)
{
    int stepSize; // A-Trous�X�e�b�v�T�C�Y (2^iteration)
    float colorSigma; // �J���[�d�ݒ���
    float normalSigma; // �@���d�ݒ��� 
    float depthSigma; // �[�x�d�ݒ���
    float2 texelSize; // �e�N�Z���T�C�Y (1.0/resolution)
    float2 padding;
}

// 5x5 A-Trous �J�[�l�� (B3-spline interpolation)
static const float kernel[49] =
{
    1.0f / 1024.0f, 3.0f / 1024.0f, 7.0f / 1024.0f, 12.0f / 1024.0f, 7.0f / 1024.0f, 3.0f / 1024.0f, 1.0f / 1024.0f,
    3.0f / 1024.0f, 9.0f / 1024.0f, 21.0f / 1024.0f, 36.0f / 1024.0f, 21.0f / 1024.0f, 9.0f / 1024.0f, 3.0f / 1024.0f,
    7.0f / 1024.0f, 21.0f / 1024.0f, 49.0f / 1024.0f, 84.0f / 1024.0f, 49.0f / 1024.0f, 21.0f / 1024.0f, 7.0f / 1024.0f,
    12.0f / 1024.0f, 36.0f / 1024.0f, 84.0f / 1024.0f, 144.0f / 1024.0f, 84.0f / 1024.0f, 36.0f / 1024.0f, 12.0f / 1024.0f,
    7.0f / 1024.0f, 21.0f / 1024.0f, 49.0f / 1024.0f, 84.0f / 1024.0f, 49.0f / 1024.0f, 21.0f / 1024.0f, 7.0f / 1024.0f,
    3.0f / 1024.0f, 9.0f / 1024.0f, 21.0f / 1024.0f, 36.0f / 1024.0f, 21.0f / 1024.0f, 9.0f / 1024.0f, 3.0f / 1024.0f,
    1.0f / 1024.0f, 3.0f / 1024.0f, 7.0f / 1024.0f, 12.0f / 1024.0f, 7.0f / 1024.0f, 3.0f / 1024.0f, 1.0f / 1024.0f
};

// 7x7�J�[�l���I�t�Z�b�g
static const int2 offsets[49] =
{
    int2(-3, -3), int2(-2, -3), int2(-1, -3), int2(0, -3), int2(1, -3), int2(2, -3), int2(3, -3),
    int2(-3, -2), int2(-2, -2), int2(-1, -2), int2(0, -2), int2(1, -2), int2(2, -2), int2(3, -2),
    int2(-3, -1), int2(-2, -1), int2(-1, -1), int2(0, -1), int2(1, -1), int2(2, -1), int2(3, -1),
    int2(-3, 0), int2(-2, 0), int2(-1, 0), int2(0, 0), int2(1, 0), int2(2, 0), int2(3, 0),
    int2(-3, 1), int2(-2, 1), int2(-1, 1), int2(0, 1), int2(1, 1), int2(2, 1), int2(3, 1),
    int2(-3, 2), int2(-2, 2), int2(-1, 2), int2(0, 2), int2(1, 2), int2(2, 2), int2(3, 2),
    int2(-3, 3), int2(-2, 3), int2(-1, 3), int2(0, 3), int2(1, 3), int2(2, 3), int2(3, 3)
};

// === �d�݌v�Z�̉��P ===
float ComputeWeight(float3 centerColor, float3 sampleColor,
                   float3 centerNormal, float3 sampleNormal,
                   float centerDepth, float sampleDepth)
{
    // �J���[�d�݁i���q���Ɂj
    float3 colorDiff = centerColor - sampleColor;
    float colorDistance = length(colorDiff);
    float colorWeight = exp(-colorDistance / max(colorSigma, 0.001f));
    
    // �@���d�݁i��茵�����j
    float normalDot = max(0.0f, dot(centerNormal, sampleNormal));
    float normalWeight = pow(normalDot, max(normalSigma, 0.001f));
    
    // �[�x�d�݁i���P�Łj
    float depthDiff = abs(centerDepth - sampleDepth);
    float depthWeight = exp(-depthDiff / max(depthSigma, 0.001f));
    
    // �g�ݍ��킹�d�݁i�o�����X�����j
    return colorWeight * normalWeight * depthWeight;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    RenderTarget.GetDimensions(dims.x, dims.y); // Common.hlsli��RenderTarget���g�p
    
    // �͈̓`�F�b�N
    if (id.x >= dims.x || id.y >= dims.y)
        return;
    
    int2 centerCoord = int2(id.xy);
    
    // ���S�s�N�Z���̒l���擾�iRenderTarget���璼�ړǂݎ��j
    float4 centerColor = RenderTarget[centerCoord];
    float3 centerAlbedo = AlbedoBuffer[centerCoord].rgb;
    float3 centerNormal = normalize(NormalBuffer[centerCoord].xyz);
    float centerDepth = DepthBuffer[centerCoord].r;
    
    // NaN/������`�F�b�N
    if (any(isnan(centerColor.rgb)) || any(isinf(centerColor.rgb)) ||
        any(isnan(centerNormal)) || any(isinf(centerNormal)) ||
        isnan(centerDepth) || isinf(centerDepth))
    {
        DenoiserOutput[id.xy] = float4(1, 0, 1, 1); // �}�[���^�ŃG���[�\��
        return;
    }
    
    // �f�t�H���g�l�̃`�F�b�N (G-Buffer����������Ă��Ȃ��ꍇ)
    if (length(centerNormal) < 0.1f || centerDepth <= 0.0f)
    {
        // G-Buffer�f�[�^���Ȃ��ꍇ�͌��̐F�����̂܂܏o��
        DenoiserOutput[id.xy] = centerColor;
        return;
    }
    
    // A-Trous�t�B���^�����O
    float4 filteredColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0f;
    
    // 5x5�J�[�l����K�p
    for (int i = 0; i < 25; ++i)
    {
        // A-Trous�X�^�C��: �X�e�b�v�T�C�Y��K�p
        int2 sampleCoord = centerCoord + offsets[i] * stepSize;
        
        // ���E�`�F�b�N
        if (sampleCoord.x < 0 || sampleCoord.x >= (int) dims.x ||
            sampleCoord.y < 0 || sampleCoord.y >= (int) dims.y)
        {
            continue;
        }
        
        // �T���v���l���擾
        float4 sampleColor = RenderTarget[sampleCoord]; // Common.hlsli��RenderTarget���g�p
        float3 sampleAlbedo = AlbedoBuffer[sampleCoord].rgb;
        float3 sampleNormal = normalize(NormalBuffer[sampleCoord].xyz);
        float sampleDepth = DepthBuffer[sampleCoord].r;
        
        // NaN/������`�F�b�N
        if (any(isnan(sampleColor.rgb)) || any(isinf(sampleColor.rgb)) ||
            any(isnan(sampleNormal)) || any(isinf(sampleNormal)) ||
            isnan(sampleDepth) || isinf(sampleDepth))
            continue;
            
        // ������G-Buffer�f�[�^�̃`�F�b�N
        if (length(sampleNormal) < 0.1f || sampleDepth <= 0.0f)
            continue;
        
        // �J�[�l���d�݂ƃG�b�W�ێ��d�݂��v�Z
        float kernelWeight = kernel[i];
        float edgeWeight = ComputeWeight(centerColor.rgb, sampleColor.rgb,
                                       centerNormal, sampleNormal,
                                       centerDepth, sampleDepth);
        
        float weight = kernelWeight * edgeWeight;
        
        filteredColor += sampleColor * weight;
        totalWeight += weight;
    }
    
    // ���K��
    if (totalWeight > 0.001f)
    {
        filteredColor /= totalWeight;
    }
    else
    {
        // �t�B���^�����O�Ɏ��s�����ꍇ�͌��̐F���g�p
        filteredColor = centerColor;
    }
    
    // �ŏI�`�F�b�N
    if (any(isnan(filteredColor.rgb)) || any(isinf(filteredColor.rgb)))
    {
        filteredColor = centerColor;
    }
    
    // �A���t�@�l��ێ�
    filteredColor.a = centerColor.a;
    
    DenoiserOutput[id.xy] = filteredColor;
}