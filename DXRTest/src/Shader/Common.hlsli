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

// **ReSTIR GI用PathVertex構造体**
struct PathVertex
{
    float3 position;     // 頂点位置
    float3 normal;       // 表面法線
    float3 albedo;       // 表面アルベド
    float3 emission;     // 自己発光
    uint materialType;   // マテリアルタイプ
    float roughness;     // 表面粗さ
    float2 padding;      // アライメント用
};

// **ReSTIR GI用完全なPath情報**
struct GIPath
{
    PathVertex vertices[4];  // 最大4バウンスのパス頂点
    float3 throughput;       // パスのスループット（累積BRDF）
    float3 radiance;         // パスの放射輝度
    uint vertexCount;        // 有効な頂点数
    float pathPdf;           // パス全体のPDF
    float misWeight;         // MIS重み
    bool valid;              // パスの有効性
};

// **ReSTIR GI用Reservoir構造体**
struct GIReservoir
{
    GIPath selectedPath;     // 選択されたGIパス
    float weight;            // Reservoir重み (W)
    uint sampleCount;        // 処理されたサンプル数 (M)
    float weightSum;         // 重みの累積 (w_sum)
    float pathPdf;           // 選択されたパスのPDF
    bool valid;              // Reservoirが有効かどうか
    float3 padding;          // アライメント用
};

// **ReSTIR DI用Reservoirバッファ（構造体定義後）**
RWStructuredBuffer<LightReservoir> CurrentReservoirs : register(u6);  // 現在フレームのReservoir
RWStructuredBuffer<LightReservoir> PreviousReservoirs : register(u7); // 前フレームのReservoir

// **ReSTIR GI用Reservoirバッファ（将来実装用 - 現在はコメントアウト）**
// RWStructuredBuffer<GIReservoir> CurrentGIReservoirs : register(u8);   // 現在フレームのGI Reservoir
// RWStructuredBuffer<GIReservoir> PreviousGIReservoirs : register(u9);  // 前フレームのGI Reservoir

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

// **ReSTIR GI用ユーティリティ関数**

// GI Reservoirの初期化
GIReservoir InitGIReservoir()
{
    GIReservoir reservoir;
    
    // GIPathの初期化
    reservoir.selectedPath.throughput = float3(0, 0, 0);
    reservoir.selectedPath.radiance = float3(0, 0, 0);
    reservoir.selectedPath.vertexCount = 0;
    reservoir.selectedPath.pathPdf = 0.0f;
    reservoir.selectedPath.misWeight = 0.0f;
    reservoir.selectedPath.valid = false;
    
    // 全頂点を初期化
    for (int i = 0; i < 4; i++) {
        reservoir.selectedPath.vertices[i].position = float3(0, 0, 0);
        reservoir.selectedPath.vertices[i].normal = float3(0, 0, 1);
        reservoir.selectedPath.vertices[i].albedo = float3(0, 0, 0);
        reservoir.selectedPath.vertices[i].emission = float3(0, 0, 0);
        reservoir.selectedPath.vertices[i].materialType = 0;
        reservoir.selectedPath.vertices[i].roughness = 0.0f;
        reservoir.selectedPath.vertices[i].padding = float2(0, 0);
    }
    
    // Reservoir状態の初期化
    reservoir.weight = 0.0f;
    reservoir.sampleCount = 0;
    reservoir.weightSum = 0.0f;
    reservoir.pathPdf = 0.0f;
    reservoir.valid = false;
    reservoir.padding = float3(0, 0, 0);
    
    return reservoir;
}

// PathVertex作成ヘルパー
PathVertex CreatePathVertex(float3 position, float3 normal, float3 albedo, 
                           float3 emission, uint materialType, float roughness)
{
    PathVertex vertex;
    vertex.position = position;
    vertex.normal = normal;
    vertex.albedo = albedo;
    vertex.emission = emission;
    vertex.materialType = materialType;
    vertex.roughness = roughness;
    vertex.padding = float2(0, 0);
    return vertex;
}

// GIPath作成ヘルパー
GIPath CreateGIPath()
{
    GIPath path;
    path.throughput = float3(1, 1, 1); // 初期スループット
    path.radiance = float3(0, 0, 0);
    path.vertexCount = 0;
    path.pathPdf = 1.0f;
    path.misWeight = 1.0f;
    path.valid = false;
    
    // 全頂点を初期化
    for (int i = 0; i < 4; i++) {
        path.vertices[i] = CreatePathVertex(float3(0,0,0), float3(0,0,1), float3(0,0,0), 
                                           float3(0,0,0), 0, 0.0f);
    }
    
    return path;
}

// GIPathの寄与度計算（MIS weight）
float CalculateGIPathContribution(GIPath path, float3 viewPos, float3 viewNormal)
{
    if (!path.valid || path.vertexCount == 0) return 0.0f;
    
    // パスの最終頂点から受け取る放射輝度
    float3 incomingRadiance = path.radiance;
    
    // パスの最初の頂点（シェーディングポイントに接続）
    PathVertex connectVertex = path.vertices[0];
    
    // 接続点への方向と距離
    float3 connectDir = connectVertex.position - viewPos;
    float connectDist = length(connectDir);
    connectDir /= connectDist;
    
    // Lambert BRDF評価
    float cosTheta = max(0.0f, dot(viewNormal, connectDir));
    if (cosTheta <= 0.0f) return 0.0f;
    
    // 距離減衰
    float attenuation = 1.0f / (connectDist * connectDist);
    
    // 放射輝度の輝度値
    float luminance = dot(incomingRadiance, float3(0.2126f, 0.7152f, 0.0722f));
    
    // 最終寄与度 = BRDF × cosine × radiance × attenuation × MIS weight
    return luminance * cosTheta * attenuation * path.misWeight;
}

// RIS (Resampled Importance Sampling) でGI Reservoir更新
bool UpdateGIReservoir(inout GIReservoir reservoir, GIPath candidatePath, 
                      float3 viewPos, float3 viewNormal, inout uint seed)
{
    // Target PDF = パスの寄与度
    float targetPdf = CalculateGIPathContribution(candidatePath, viewPos, viewNormal);
    float sourcePdf = candidatePath.pathPdf;
    
    if (sourcePdf <= 0.0f || targetPdf <= 0.0f) return false;
    
    // 重み計算 (w = target_pdf / source_pdf)
    float weight = targetPdf / sourcePdf;
    reservoir.weightSum += weight;
    reservoir.sampleCount++;
    
    // 重み付き確率でサンプル選択
    float probability = weight / reservoir.weightSum;
    if (RandomFloat(seed) < probability) {
        reservoir.selectedPath = candidatePath;
        reservoir.pathPdf = targetPdf;
        reservoir.valid = true;
    }
    
    // 最終重み更新 (W = (1/M) * (w_sum / p_hat))
    // ReSTIR論文に従った正しい重み計算（GI用はpathPdfを使用）
    if (reservoir.sampleCount > 0 && reservoir.pathPdf > 0.0f) {
        reservoir.weight = reservoir.weightSum / (float(reservoir.sampleCount) * reservoir.pathPdf);
    }
    
    return true;
}

// 2つのGI Reservoirを結合
GIReservoir CombineGIReservoirs(GIReservoir r1, GIReservoir r2, 
                               float3 viewPos, float3 viewNormal, inout uint seed)
{
    if (!r1.valid) return r2;
    if (!r2.valid) return r1;
    
    GIReservoir result = r1;
    
    // r2のパスを考慮してReservoir更新
    UpdateGIReservoir(result, r2.selectedPath, viewPos, viewNormal, seed);
    
    return result;
}

// ライト関連ヘッダーをインクルード（循環参照を避けるため最後に）
#include "LightFunc.hlsli"

#endif // COMMON_HLSLI