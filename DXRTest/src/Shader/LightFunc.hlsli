#ifndef LIGHTFUNC_HLSLI
#define LIGHTFUNC_HLSLI
#include "Utils.hlsli"

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

// MIS重み計算（Power Heuristic）
float PowerHeuristic(int nf, float fPdf, int ng, float gPdf)
{
    float f = nf * fPdf;
    float g = ng * gPdf;
    return (f * f) / (f * f + g * g);
}

// バランスヒューリスティック（β=1の場合）
float BalanceHeuristic(float lightPdf, float brdfPdf)
{
    return lightPdf / (lightPdf + brdfPdf);
}

// パワーヒューリスティック（β=2の場合）
float PowerHeuristic2(float lightPdf, float brdfPdf)
{
    float lightSq = lightPdf * lightPdf;
    float brdfSq = brdfPdf * brdfPdf;
    return lightSq / (lightSq + brdfSq);
}

// MIS重み計算（ライトサンプリング用）
float MISWeightLight(float lightPdf, float brdfPdf)
{
    if (lightPdf <= 0.0f) return 0.0f;
    if (brdfPdf <= 0.0f) return 1.0f;
    
    // パワーヒューリスティック（β=2）を使用
    return PowerHeuristic2(lightPdf, brdfPdf);
}

// MIS重み計算（BRDFサンプリング用）
float MISWeightBRDF(float lightPdf, float brdfPdf)
{
    if (brdfPdf <= 0.0f) return 0.0f;
    if (lightPdf <= 0.0f) return 1.0f;
    
    // パワーヒューリスティック（β=2）を使用
    return PowerHeuristic2(brdfPdf, lightPdf);
}

// 指定されたライトに対するPDF計算
float CalculateLightPDF(uint lightIndex, float3 worldPos, float3 lightSamplePos, float3 lightDirection)
{
    if (lightIndex >= numLights) return 0.0f;
    
    LightInfo light = GetLightByIndex(lightIndex);
    
    if (light.lightType == 0) { // エリアライト
        float distance = length(lightSamplePos - worldPos);
        float cosTheta = max(0.0f, dot(-lightDirection, light.normal));
        
        if (cosTheta > 0.001f && distance > 0.001f) {
            float distanceSquared = distance * distance;
            return distanceSquared / (cosTheta * light.area);
        }
    }
    else if (light.lightType == 1) { // ポイントライト
        float distance = length(light.position - worldPos);
        if (distance > 0.001f) {
            float distanceSquared = distance * distance;
            return 1.0f / (4.0f * PI * distanceSquared);
        }
    }
    
    return 0.0f;
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

// **Stratified Sampling対応版：エリアライトの均等分割サンプリング**
LightSample SampleLightByIndexStratified(uint lightIndex, float3 worldPos, inout uint seed, uint stratumX, uint stratumY, uint strataCountX, uint strataCountY)
{
    LightSample sample;
    sample.valid = false;
    
    if (lightIndex >= numLights) {
        return sample;
    }
    
    LightInfo light = GetLightByIndex(lightIndex);
    
    if (light.lightType == 0) { // エリアライト
        // **Stratified Sampling: ライト表面を格子分割してセル内でランダムサンプリング**
        float u = RandomFloat(seed);
        float v = RandomFloat(seed);
        
        // 各ストラタム（セル）内での相対座標 [0,1]
        float cellU = (float(stratumX) + u) / float(strataCountX);
        float cellV = (float(stratumY) + v) / float(strataCountY);
        
        // **保守的サンプリング: ライトの端部を避ける（90%領域のみ使用）**
        float safeMargin = 0.05f; // 5%のマージン
        cellU = safeMargin + cellU * (1.0f - 2.0f * safeMargin);
        cellV = safeMargin + cellV * (1.0f - 2.0f * safeMargin);
        
        // ライト表面座標に変換（薄いライトでもY方向を適切にサンプリング）
        float3 lightSamplePos = light.position + float3(
            (cellU - 0.5f) * light.size.x,
            (v - 0.5f) * light.size.y, // **修正: Y方向も適切にサンプリング**
            (cellV - 0.5f) * light.size.z
        );
        
        float3 toLight = lightSamplePos - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = lightSamplePos;
        
        float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
        
        if (sample.distance > 0.001f && cosTheta > 0.01f) { // **より厳しいcosTheta閾値**
            float distanceSquared = sample.distance * sample.distance;
            float rawPdf = distanceSquared / (cosTheta * light.area);
            
            // **PDF値の上限制限（異常値防止）**
            sample.pdf = min(rawPdf, 1000.0f); // PDFの最大値を制限
            sample.radiance = light.emission;
            sample.valid = true;
        }
    }
    else if (light.lightType == 1) { // ポイントライト
        float3 toLight = light.position - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = light.position;
        
        if (sample.distance > 0.001f) {
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = 1.0f / (4.0f * PI * distanceSquared);
            sample.radiance = light.emission / distanceSquared;
            sample.valid = true;
        }
    }
    
    return sample;
}

// 動的ライト対応：指定されたライトをサンプリング
LightSample SampleLightByIndex(uint lightIndex, float3 worldPos, inout uint seed)
{
    LightSample sample;
    sample.valid = false;
    
    if (lightIndex >= numLights) {
        return sample;
    }
    
    LightInfo light = GetLightByIndex(lightIndex);
    
    float u = RandomFloat(seed);
    float v = RandomFloat(seed);
    
    if (light.lightType == 0) { // エリアライト
        // XZ平面での矩形サンプリング
        float3 lightSamplePos = light.position + float3(
            (u - 0.5f) * light.size.x,
            0.0f,
            (v - 0.5f) * light.size.z
        );
        
        float3 toLight = lightSamplePos - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = lightSamplePos;
        
        float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
        
        // 物理的に正確なチェック：距離と法線角度
        if (sample.distance > 0.001f && cosTheta > 0.001f) {
            // **物理的に正確なエリアライト実装**
            // PDF: 立体角変換（面積→立体角）
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = distanceSquared / (cosTheta * light.area);
            
            // **物理的に正確な放射輝度**
            // エリアライトは距離に関係なく一定の放射輝度
            sample.radiance = light.emission;
            sample.valid = true;
        }
    }
    else if (light.lightType == 1) { // ポイントライト
        float3 toLight = light.position - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = light.position;
        
        if (sample.distance > 0.001f) {
            // ポイントライトのPDF: 1 / (4π * r²)
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = 1.0f / (4.0f * PI * distanceSquared);
            sample.radiance = light.emission / distanceSquared; // 距離減衰
            sample.valid = true;
        }
    }
    
    return sample;
}

// **Poisson Disk Sampling用のプリセット座標（8サンプル）**
static const float2 PoissonDisk8[8] = {
    float2(-0.7071f, 0.7071f),   // 左上
    float2(-0.0000f, 1.0000f),   // 上
    float2(0.7071f, 0.7071f),    // 右上
    float2(1.0000f, 0.0000f),    // 右
    float2(0.7071f, -0.7071f),   // 右下
    float2(0.0000f, -1.0000f),   // 下
    float2(-0.7071f, -0.7071f),  // 左下
    float2(-1.0000f, 0.0000f)    // 左
};

// **Poisson Disk Sampling版：エリアライトの自然分布サンプリング**
LightSample SampleLightByIndexPoisson(uint lightIndex, float3 worldPos, inout uint seed, uint sampleIdx, uint totalSamples)
{
    LightSample sample;
    sample.valid = false;
    
    if (lightIndex >= numLights || sampleIdx >= totalSamples) {
        return sample;
    }
    
    LightInfo light = GetLightByIndex(lightIndex);
    
    if (light.lightType == 0) { // エリアライト
        // **Poisson Disk Sampling: より自然な分布**
        float2 diskOffset;
        
        if (totalSamples <= 8) {
            // プリセットのPoisson Diskパターンを使用
            diskOffset = PoissonDisk8[sampleIdx % 8];
        } else {
            // ランダムなPoisson-like分布（簡易版）
            float angle = (float(sampleIdx) / float(totalSamples)) * 2.0f * PI + RandomFloat(seed) * 0.5f;
            float radius = sqrt(RandomFloat(seed)) * 0.8f; // 0.8fで境界を避ける
            diskOffset = float2(cos(angle), sin(angle)) * radius;
        }
        
        // ランダム回転でパターンを変える
        float rotation = RandomFloat(seed) * 2.0f * PI;
        float cosRot = cos(rotation);
        float sinRot = sin(rotation);
        float2 rotatedOffset = float2(
            diskOffset.x * cosRot - diskOffset.y * sinRot,
            diskOffset.x * sinRot + diskOffset.y * cosRot
        );
        
        // ライト表面座標に変換（-0.4 ~ +0.4の範囲に制限）
        float3 lightSamplePos = light.position + float3(
            rotatedOffset.x * light.size.x * 0.4f,
            0.0f,
            rotatedOffset.y * light.size.z * 0.4f
        );
        
        float3 toLight = lightSamplePos - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = lightSamplePos;
        
        float cosTheta = max(0.0f, dot(-sample.direction, light.normal));
        
        if (sample.distance > 0.001f && cosTheta > 0.001f) {
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = distanceSquared / (cosTheta * light.area);
            sample.radiance = light.emission;
            sample.valid = true;
        }
    }
    else if (light.lightType == 1) { // ポイントライト
        float3 toLight = light.position - worldPos;
        sample.distance = length(toLight);
        sample.direction = normalize(toLight);
        sample.position = light.position;
        
        if (sample.distance > 0.001f) {
            float distanceSquared = sample.distance * sample.distance;
            sample.pdf = 1.0f / (4.0f * PI * distanceSquared);
            sample.radiance = light.emission / distanceSquared;
            sample.valid = true;
        }
    }
    
    return sample;
}

// ランダムライト選択：重要度サンプリング（簡易版：均等選択）
// LightData.hlsliで宣言されている関数の実装
LightSample SampleRandomLight(float3 worldPos, inout uint seed)
{
    if (numLights == 0) {
        LightSample invalidSample;
        invalidSample.valid = false;
        return invalidSample;
    }
    
    // 現在は均等選択、将来的には重要度ベースに拡張
    uint lightIndex = uint(RandomFloat(seed) * float(numLights)) % numLights;
    return SampleLightByIndex(lightIndex, worldPos, seed);
}


#endif