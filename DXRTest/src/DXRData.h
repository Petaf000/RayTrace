#pragma once
// DXR�p�̊g�����_�\����
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

// BLAS�\�z�p�f�[�^
struct BLASData {
    ComPtr<ID3D12Resource> vertexBuffer;
    std::vector<DXRVertex> vertices;
    std::vector<uint32_t> indices;
    ComPtr<ID3D12Resource> indexBuffer;
    uint32_t materialID;
    XMMATRIX transform;
    ComPtr<ID3D12Resource> scratchBuffer;
};

// TLAS�\�z�p�f�[�^  
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

// **ReSTIR GI用構造体（将来実装用）**
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

struct GIReservoir {
    GIPath selectedPath;     // 選択されたGIパス (296 bytes)
    float weight;            // Reservoir重み (4 bytes)
    uint32_t sampleCount;    // 処理されたサンプル数 (4 bytes)
    float weightSum;         // 重みの累積 (4 bytes)
    float pathPdf;           // 選択されたパスのPDF (4 bytes)
    uint32_t valid;          // Reservoirが有効かどうか (4 bytes)
    XMFLOAT3 padding;        // アライメント用 (12 bytes)
    // 合計: 328 bytes
};

// ������ DXR�ݒ�萔 ������
namespace DXRConfig {
    static constexpr UINT MAX_INSTANCES = 128;           // �ő�C���X�^���X��
    static constexpr UINT MAX_MATERIALS = 64;            // �ő�}�e���A����
    static constexpr UINT MAX_LIGHTS = 32;               // 最大ライト数
    static constexpr UINT MAX_RAY_DEPTH = 8;             // �ő僌�C�̐[�x
    static constexpr UINT SHADER_IDENTIFIER_SIZE = 32;   // �V�F�[�_�[���ʎq�T�C�Y
}