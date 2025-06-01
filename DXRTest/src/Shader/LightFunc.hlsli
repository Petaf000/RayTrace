#ifndef LIGHTFUNC_HLSLI
#define LIGHTFUNC_HLSLI
#include "Utils.hlsli"
#include "LightData.hlsli"

// PDF�l�̌v�Z�iCPU���C�g���Ɠ��������j
float LightSamplePDF(float3 worldPos, float3 direction, float distance)
{
    LightInfo light = GetLightInfo();
    
    // ���C�ƌ����̌������e�X�g
    // �ȗ����F������������ւ̓��B���m�F
    float3 lightCenter = light.position;
    float3 toLight = normalize(lightCenter - worldPos);
    
    // �����̗ގ������`�F�b�N
    if (dot(normalize(direction), toLight) > 0.99f) // ��̓�������
    {
        float distanceSquared = distance * distance;
        float cosTheta = max(0.0f, dot(-normalize(direction), light.normal));
        if (cosTheta > 0.0f)
        {
            return distanceSquared / (light.area * cosTheta);
        }
    }
    
    return 0.0f;
}

float CosinePDF(float3 direction, float3 normal)
{
    float cosTheta = max(0.0f, dot(direction, normal));
    return cosTheta / PI;
}

// MIS�d�݌v�Z�iPower Heuristic�j
float PowerHeuristic(int nf, float fPdf, int ng, float gPdf)
{
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f * f) / (f * f + g * g);
}

// ������ CPU�R�[�h�����̃G���A���C�g�T���v�����O ������
LightSample SampleAreaLight(float3 worldPos, inout uint seed)
{
    LightSample sample;
    LightInfo light = GetLightInfo();
    
    float u = RandomFloat(seed);
    float v = RandomFloat(seed);
    
    // XZ���ʂł̋�`�T���v�����O
    float3 lightSamplePos = light.position + float3(
        (u - 0.5f) * light.size.x,
        0.0f,
        (v - 0.5f) * light.size.z
    );
    
    // �����Ƌ���
    float3 toLight = lightSamplePos - worldPos;
    sample.distance = length(toLight);
    sample.direction = normalize(toLight);
    sample.position = lightSamplePos;
    
    // ���C�g�̖@���Ƃ̓��ρi���������C�g�j
    float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
    
    if (cosTheta > 0.001f && sample.distance > 0.001f)
    {
        // ������ �C��: ���������̊pPDF�i�����������݁j ������
        float distanceSquared = sample.distance * sample.distance;
        sample.pdf = distanceSquared / (cosTheta * light.area);
        
        // ������ ���S�Ȕ͈͂ɃN�����v ������
        sample.pdf = max(sample.pdf, 0.0001f); // �ŏ�PDF�l
        
        sample.radiance = light.emission;
        sample.valid = true;
    }
    else
    {
        sample.valid = false;
    }
    
    return sample;
}


#endif