#pragma once
// DXR用の拡張頂点構造体
struct DXRVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texCoord;
};

// DXR用マテリアルデータ
struct DXRMaterialData {
    XMFLOAT3 albedo;
    float roughness;        // Metal用
    float refractiveIndex;  // Dielectric用  
    XMFLOAT3 emission;      // DiffuseLight用
    int materialType;       // 0=Lambertian, 1=Metal, 2=Dielectric, 3=Light
    float padding[1];       // アライメント調整
};

// BLAS構築用データ
struct BLASData {
    ComPtr<ID3D12Resource> vertexBuffer;
    std::vector<DXRVertex> vertices;
    std::vector<uint32_t> indices;
    ComPtr<ID3D12Resource> indexBuffer;
    uint32_t materialID;
    XMMATRIX transform;
    ComPtr<ID3D12Resource> scratchBuffer;
};

// TLAS構築用データ  
struct TLASData {
    std::vector<BLASData> blasDataList;
    std::vector<XMMATRIX> instanceTransforms;
    ComPtr<ID3D12Resource> instanceBuffer;
};

// 動的ライト情報構造体
struct DXRLightData {
    XMFLOAT3 position;      // ライトの位置
    float padding1;         // アライメント用
    XMFLOAT3 emission;      // 放射輝度
    float padding2;         // アライメント用
    XMFLOAT3 size;          // エリアライトのサイズ (x, y, z)
    float area;             // エリアライトの面積
    XMFLOAT3 normal;        // エリアライトの法線
    uint32_t lightType;     // ライトタイプ (0=area, 1=point, 2=directional)
    uint32_t instanceID;    // 対応するインスタンスID
    float lightPadding[3];  // アライメント用（paddingと名前重複回避）
};

// **ReSTIR DI用Reservoir構造体（C++側）**
struct LightReservoir {
    uint32_t lightIndex;     // 選択されたライトのインデックス (4 bytes)
    XMFLOAT3 samplePos;      // ライト上のサンプル位置 (12 bytes)
    XMFLOAT3 radiance;       // 放射輝度 (12 bytes)
    float weight;            // Reservoir重み (W) (4 bytes)
    uint32_t sampleCount;    // 処理されたサンプル数 (M) (4 bytes)
    float weightSum;         // 重みの累積 (w_sum) (4 bytes)
    float pdf;               // サンプルのPDF (4 bytes)
    uint32_t valid;          // Reservoirが有効かどうか (4 bytes, bool -> uint32_t)
    float padding;           // アライメント用 (4 bytes)
    // 合計: 52 bytes
};

// **ReSTIR GI用Reservoir構造体**
struct GIReservoir {
    XMFLOAT3 position;       // サンプル位置 (12 bytes)
    float padding1;          // アライメント (4 bytes)
    XMFLOAT3 normal;         // サンプル法線 (12 bytes)
    float padding2;          // アライメント (4 bytes)
    XMFLOAT3 radiance;       // サンプル輝度 (12 bytes)
    float padding3;          // アライメント (4 bytes)
    XMFLOAT3 throughput;     // 経路throughput (12 bytes)
    float weight;            // RIS重み (4 bytes)
    float weightSum;         // 重み累積 (4 bytes)
    uint32_t sampleCount;    // サンプル数（M値） (4 bytes)
    float pdf;               // PDF値 (4 bytes)
    uint32_t valid;          // 有効性フラグ (4 bytes)
    uint32_t pathLength;     // パス長 (4 bytes)
    uint32_t bounceCount;    // バウンス回数 (4 bytes)
    XMFLOAT3 albedo;         // アルベド (12 bytes)
    float padding4;          // アライメント (4 bytes)
    // 合計: 96 bytes
};

// **パストレーシング用頂点構造体（将来実装用）**
struct PathVertex {
    XMFLOAT3 position;       // 頂点位置 (12 bytes)
    XMFLOAT3 normal;         // 表面法線 (12 bytes) 
    XMFLOAT3 albedo;         // 表面アルベド (12 bytes)
    XMFLOAT3 emission;       // 自己発光 (12 bytes)
    uint32_t materialType;   // マテリアルタイプ (4 bytes)
    float roughness;         // 表面粗さ (4 bytes)
    XMFLOAT2 padding;        // アライメント用 (8 bytes)
    // 合計: 64 bytes
};

struct GIPath {
    PathVertex vertices[4];  // 最大4バウンスのパス頂点 (256 bytes)
    XMFLOAT3 throughput;     // パスのスループット (12 bytes)
    XMFLOAT3 radiance;       // パスの放射輝度 (12 bytes)
    uint32_t vertexCount;    // 有効な頂点数 (4 bytes)
    float pathPdf;           // パス全体のPDF (4 bytes)
    float misWeight;         // MIS重み (4 bytes)
    uint32_t valid;          // パスの有効性 (4 bytes, bool -> uint32_t)
    // 合計: 296 bytes
};

// GIReservoir構造体は上記（66行目）で既に定義済み

// 共通の DXR設定定数 
namespace DXRConfig {
    static constexpr UINT MAX_INSTANCES = 128;           // 最大インスタンス数
    static constexpr UINT MAX_MATERIALS = 64;            // 最大マテリアル数
    static constexpr UINT MAX_LIGHTS = 32;               // 最大ライト数
    static constexpr UINT MAX_RAY_DEPTH = 8;             // 最大レイの深度
    static constexpr UINT SHADER_IDENTIFIER_SIZE = 32;   // シェーダー識別子サイズ
}