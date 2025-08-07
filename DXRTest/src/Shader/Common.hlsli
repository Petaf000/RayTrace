// shader/Common.hlsli - ���ʃw�b�_�[�i�f�o�b�O�Łj
#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#include "Utils.hlsli"
#include "GeometryData.hlsli"

// �萔�o�b�t�@
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    
    // �p�b�N���ꂽ�`���i�A�N�Z�X���₷���d���j
    float3 cameraRight;
    float tanHalfFov; // cameraRight�Ɠ���float4�Ɏ��߂�
    
    float3 cameraUp;
    float aspectRatio; // cameraUp�Ɠ���float4�Ɏ��߂�
    
    float3 cameraForward;
    float frameCount; // �����̊g���p�i�A�j���[�V�������j
    
    uint numLights;   // ライト数
    uint cameraMovedFlag; // カメラ動き検出フラグ (0=静止, 1=動いた)
    float2 padding;   // アライメント用
    
    // 合計: 128 + 48 + 16 = 192 bytes (完全にアライメント)
};

// �O���[�o�����\�[�X
RaytracingAccelerationStructure SceneBVH : register(t0);
RWTexture2D<float4> RenderTarget : register(u0);

// **時間的蓄積用テクスチャ**
RWTexture2D<float4> AccumulationBuffer : register(u1); // RGB: 蓄積色, A: フレーム数
RWTexture2D<float4> PrevFrameData : register(u2);      // 前フレームの情報（位置、法線など）

// **G-Buffer出力用テクスチャ**
RWTexture2D<float4> AlbedoOutput : register(u3);  // アルベド出力
RWTexture2D<float4> NormalOutput : register(u4);  // 法線出力
RWTexture2D<float4> DepthOutput : register(u5);   // 深度出力

// ライト関連構造体の定義（StructuredBufferの前に必要）
struct LightInfo
{
    float3 position;       // ライトの位置
    float padding1;        // アライメント用
    float3 emission;       // 放射輝度
    float padding2;        // アライメント用
    float3 size;           // エリアライトのサイズ (x, y, z)
    float area;            // エリアライトの面積
    float3 normal;         // エリアライトの法線
    uint lightType;        // ライトタイプ (0=area, 1=point, 2=directional)
    uint instanceID;       // 対応するインスタンスID
    float3 lightPadding;   // アライメント用（paddingと名前重複回避）
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

// **ReSTIR用Reservoir構造体**
struct LightReservoir
{
    uint lightIndex;     // 選択されたライトのインデックス
    float3 samplePos;    // ライト上のサンプル位置
    float3 radiance;     // 放射輝度
    float weight;        // Reservoir重み (W)
    uint sampleCount;    // 処理されたサンプル数 (M)
    float weightSum;     // 重みの累積 (w_sum)
    float pdf;           // サンプルのPDF
    bool valid;          // Reservoirが有効かどうか
    float padding;       // アライメント用
};

struct PathReservoir
{
    float3 pathVertex;   // パス上の頂点位置
    float3 pathDirection;// パス方向
    float3 pathRadiance; // パス放射輝度
    float pathLength;    // パス長
    float weight;        // Reservoir重み
    uint bounceCount;    // バウンス回数
    float pdf;           // パスPDF
    bool valid;          // 有効性
};

// **ReSTIR GI用構造体は削除（メモリリーク対策）**
// PathVertex, GIPath, GIReservoir構造体を削除してメモリリーク回避

// **ReSTIR DI用Reservoirバッファ（構造体定義後）**
RWStructuredBuffer<LightReservoir> CurrentReservoirs : register(u6);  // 現在フレームのReservoir
RWStructuredBuffer<LightReservoir> PreviousReservoirs : register(u7); // 前フレームのReservoir

// **ReSTIR GI用Reservoirバッファは削除（メモリリーク対策）**
// u8, u9レジスタ使用を完全停止してメモリリーク回避

// ������ �ǉ��F�e��o�b�t�@ ������
StructuredBuffer<MaterialData> MaterialBuffer : register(t1);
StructuredBuffer<DXRVertex> VertexBuffer : register(t2);
StructuredBuffer<uint> IndexBuffer : register(t3);
StructuredBuffer<InstanceOffsetData> InstanceOffsetBuffer : register(t4);
StructuredBuffer<LightInfo> LightBuffer : register(t5);

// ������ �ǉ��F�}�e���A���擾�w���p�[�֐� ������
MaterialData GetMaterial(uint instanceID)
{
    InstanceOffsetData instanceData = InstanceOffsetBuffer[instanceID];
    
    // TODO: CPP���Ń}�e���A�����Ȃ����̏�����ǉ�����
    /*if (instanceData.materialID == 0xFFFFFFFF)
    {
        // �f�t�H���g�}�e���A��
        MaterialData defaultMaterial;
        defaultMaterial.albedo = float3(0.0f, 0.0f, 1.0f); // 青（エラー色）
        defaultMaterial.roughness = 1.0f;
        defaultMaterial.refractiveIndex = 1.0f;
        defaultMaterial.emission = float3(0, 0, 0);
        defaultMaterial.materialType = MATERIAL_LAMBERTIAN;
        defaultMaterial.padding = 0.0f;
        return defaultMaterial;
    }*/
    return MaterialBuffer[instanceData.materialID];
}

// ������ �f�o�b�O�p�F�@����F�ɕϊ�����֐� ������
float3 NormalToColor(float3 normal)
{
    // �@���x�N�g�� (-1 to 1) ��F (0 to 1) �ɕϊ�
    return normal * 0.5f + 0.5f;
}
// ������ �ł��V���v���Ȗ@���擾�i�f�o�b�O�p�j������
float3 GetInterpolatedNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    // �I�t�Z�b�g�����擾
    InstanceOffsetData offset = InstanceOffsetBuffer[instanceID];
    
    // �O�p�`�̃C���f�b�N�X���擾  
    uint baseIndex = offset.indexOffset + primitiveID * 3;
    
    // ���_�C���f�b�N�X�擾
    uint i0 = IndexBuffer[baseIndex + 0] + offset.vertexOffset;
    uint i1 = IndexBuffer[baseIndex + 1] + offset.vertexOffset;
    uint i2 = IndexBuffer[baseIndex + 2] + offset.vertexOffset;
    
    // ���_�@�����d�S���W�ŕ��
    float3 normal = VertexBuffer[i0].normal * (1.0f - barycentrics.x - barycentrics.y) +
                    VertexBuffer[i1].normal * barycentrics.x +
                    VertexBuffer[i2].normal * barycentrics.y;
    
    return normalize(normal);
}

// ������ ���[���h�ϊ��ł̖@���擾�i�C���Łj ������
float3 GetWorldNormal(uint instanceID, uint primitiveID, float2 barycentrics)
{
    // ���[�J���@�����擾
    float3 localNormal = GetInterpolatedNormal(instanceID, primitiveID, barycentrics);

    // ���[���h�ϊ��s����擾
    float3x4 objectToWorld = ObjectToWorld3x4();

    // �@�������[���h��Ԃɕϊ�
    // objectToWorld�̍����3x3�����i��]�ƃX�P�[�����O��S���j����Z����
    float3 worldNormal = mul((float3x3) objectToWorld, localNormal);

    // �ϊ���̖@���𐳋K�����ĕԂ�
    return normalize(worldNormal);
}

// ライト関連関数の実装
LightInfo GetLightByIndex(uint lightIndex)
{
    return LightBuffer[lightIndex];
}

LightInfo GetLightInfo()
{
    // 互換性のため、最初のライトを返す（ライトが存在する場合）
    if (numLights > 0) {
        return GetLightByIndex(0);
    }
    
    // フォールバック：デフォルトライト
    LightInfo light;
    light.position = float3(0.0f, 267.5f, -227.0f);
    light.emission = float3(15.0f, 15.0f, 15.0f);
    light.size = float3(130.0f, 5.0f, 105.0f);
    light.area = light.size.x * light.size.z;
    light.normal = float3(0, -1, 0);
    light.lightType = 0; // エリアライト
    light.instanceID = 5;
    return light;
}


// **ReSTIR Reservoir操作関数**

// Reservoirの初期化
LightReservoir InitLightReservoir()
{
    LightReservoir reservoir;
    reservoir.lightIndex = 0;
    reservoir.samplePos = float3(0, 0, 0);
    reservoir.radiance = float3(0, 0, 0);
    reservoir.weight = 0.0f;
    reservoir.sampleCount = 0;
    reservoir.weightSum = 0.0f;
    reservoir.pdf = 0.0f;
    reservoir.valid = false;
    reservoir.padding = 0.0f;
    return reservoir;
}

// RIS (Resampled Importance Sampling) でサンプル更新
bool UpdateLightReservoir(inout LightReservoir reservoir, uint lightIdx, float3 samplePos, 
                         float3 radiance, float targetPdf, float sourcePdf, inout uint seed)
{
    if (sourcePdf <= 0.0f || targetPdf <= 0.0f) return false;
    
    // 重み計算 (w = target_pdf / source_pdf)
    float weight = targetPdf / sourcePdf;
    reservoir.weightSum += weight;
    reservoir.sampleCount++;
    
    // 重み付き確率でサンプル選択
    float probability = weight / reservoir.weightSum;
    if (RandomFloat(seed) < probability) {
        reservoir.lightIndex = lightIdx;
        reservoir.samplePos = samplePos;
        reservoir.radiance = radiance;
        reservoir.pdf = targetPdf;
        reservoir.valid = true;
    }
    
    // 最終重み更新 (W = (1/M) * (w_sum / p_hat))
    // ReSTIR論文に従った正しい重み計算
    if (reservoir.sampleCount > 0 && reservoir.pdf > 0.0f) {
        reservoir.weight = reservoir.weightSum / (float(reservoir.sampleCount) * reservoir.pdf);
    }
    
    return true;
}

// 2つのReservoirを結合
LightReservoir CombineLightReservoirs(LightReservoir r1, LightReservoir r2, inout uint seed)
{
    if (!r1.valid) return r2;
    if (!r2.valid) return r1;
    
    LightReservoir result = r1;
    
    // r2のサンプルを考慮して更新
    result.weightSum += r2.weightSum;
    result.sampleCount += r2.sampleCount;
    
    float probability = r2.weightSum / result.weightSum;
    if (RandomFloat(seed) < probability) {
        result.lightIndex = r2.lightIndex;
        result.samplePos = r2.samplePos;
        result.radiance = r2.radiance;
        result.pdf = r2.pdf;
    }
    
    // 最終重み計算
    if (result.sampleCount > 0) {
        result.weight = result.weightSum / float(result.sampleCount);
    }
    
    return result;
}

// **ReSTIR DI用ユーティリティ関数**

// ピクセル座標をReservoirインデックスに変換
uint GetReservoirIndex(uint2 pixelCoord, uint2 screenDim)
{
    return pixelCoord.y * screenDim.x + pixelCoord.x;
}

// ライト重要度サンプリング（MISのtarget PDF計算）
float CalculateLightTargetPDF(float3 worldPos, float3 normal, LightInfo light, float3 samplePos)
{
    float3 lightDir = samplePos - worldPos;
    float lightDist = length(lightDir);
    lightDir /= lightDist;
    
    // Lambert BRDF cosine term
    float cosTheta = max(0.0f, dot(normal, lightDir));
    if (cosTheta <= 0.0f) return 0.0f;
    
    // 距離の二乗による減衰
    float attenuation = 1.0f / (lightDist * lightDist);
    
    // 光源の放射輝度
    float3 radiance = light.emission;
    float luminance = dot(radiance, float3(0.2126f, 0.7152f, 0.0722f));
    
    // MIS重み = BRDF × cosine × radiance × attenuation
    return luminance * cosTheta * attenuation;
}

// エリアライト上の均等サンプリング
float3 SampleAreaLight(LightInfo light, float2 random, out float pdf)
{
    // エリアライトの中心位置から相対オフセット計算
    float3 offset = float3(
        (random.x - 0.5f) * light.size.x,
        0.0f,
        (random.y - 0.5f) * light.size.z
    );
    
    // サンプル位置計算
    float3 samplePos = light.position + offset;
    
    // PDF = 1 / Area (均等サンプリング)
    pdf = 1.0f / light.area;
    
    return samplePos;
}

// ReSTIR DIでの初期候補生成
LightReservoir GenerateInitialLightSample(float3 worldPos, float3 normal, inout uint seed)
{
    LightReservoir reservoir = InitLightReservoir();
    
    if (numLights == 0) return reservoir;
    
    // ランダムにライトを選択（将来的には重要度サンプリングで改善）
    uint lightIndex = uint(RandomFloat(seed) * float(numLights)) % numLights;
    LightInfo light = GetLightByIndex(lightIndex);
    
    // エリアライト上でサンプリング
    float2 random = float2(RandomFloat(seed), RandomFloat(seed));
    float sourcePdf;
    float3 samplePos = SampleAreaLight(light, random, sourcePdf);
    
    // Target PDF計算（MIS重み）
    float targetPdf = CalculateLightTargetPDF(worldPos, normal, light, samplePos);
    
    // 放射輝度計算（cosineは後でBRDF評価時に計算するため、ここでは生の放射輝度のみ）
    float3 radiance = light.emission;
    
    // Reservoirに候補追加
    UpdateLightReservoir(reservoir, lightIndex, samplePos, radiance, targetPdf, sourcePdf, seed);
    
    return reservoir;
}

// **ReSTIR GI関連関数は削除（メモリリーク対策）**
// 全てのGI Reservoir関数を削除してクリーンな状態に戻す


// ライト関連ヘッダーをインクルード（循環参照を避けるため最後に）
#include "LightFunc.hlsli"

#endif // COMMON_HLSLI