// DXRRenderer.h
#pragma once

#include "DXRData.h"

class Renderer;

class DXRRenderer {
public:
    void Init(Renderer* renderer);
    void UnInit();
    void Render();
	void RenderDXRIMGUI();

	UINT GetWidth() const { return m_width; }
	UINT GetHeight() const { return m_height; }
	
    void SetDenoiserEnabled(bool enabled) { m_denoiserEnabled = enabled; }
    void SetDenoiserIterations(int iterations) { m_denoiserIterations = max(1, min(5, iterations)); }
    void SetDenoiserParameters(float colorSigma, float normalSigma, float depthSigma) {
        m_colorSigma = colorSigma;
        m_normalSigma = normalSigma;
        m_depthSigma = depthSigma;
    }
private:
    friend class Singleton<DXRRenderer>;

    DXRRenderer() = default;
    ~DXRRenderer() = default;

    // DXR初期化
    void InitializeDXR(ID3D12Device* device);
    void CreateRootSignature();
    void CreateRaytracingPipelineStateObject();
    void CreateShaderTables();
    void CreateOutputResource();
    void CreateMaterialBuffer(const TLASData& tlasData);
    void CreateLightBuffer(const TLASData& tlasData);
    void CollectLightsFromScene(const TLASData& tlasData);

    // 加速構造作成
    void CreateAccelerationStructures();
    void CreateBLAS(BLASData& blasData, ComPtr<ID3D12Resource>& blasBuffer);
    void CreateTLAS(TLASData& tlasData);
    //void CreateDescriptorsForBuffers(const UINT materialCount, const TLASData& tlasData);
    void CreateVertexIndexBuffers(const TLASData& tlasData);
    void CreateInstanceOffsetBuffer(const TLASData& tlasData, const std::vector<uint32_t>& vertexOffsets, const std::vector<uint32_t>& indexOffsets);

	// IMGUI用ディスクリプタヒープ作成
    void CreateDebugBufferViews();

    // デノイザー関連メソッド
    void CreateDenoiserResources();
    void CreateDenoiserPipeline();
    ID3D12Resource* RunDenoiser();
    void UpdateDenoiserConstants(int stepSize);
    
    // **ReSTIR関連メソッド**
    void CreateReSTIRResources();
    void InitializeReSTIRBuffers();

    // DXCCompiler
    // 実行時HLSLコンパイル関数
    ComPtr<IDxcBlob> CompileShaderFromFile(const std::wstring& hlslPath,
        const std::wstring& entryPoint,
        const std::wstring& target = L"lib_6_5");

    // キャッシュ機能付きリロード関数
    ComPtr<IDxcBlob> LoadOrCompileShader(const std::wstring& hlslPath,
        const std::wstring& entryPoint,
        const std::wstring& target = L"lib_6_5");

    // シェーダー関連
    ComPtr<IDxcBlob> LoadCSO(const std::wstring& filename);

    // ヘルパー関数
    void UpdateCamera();
    UINT AlignTo(UINT size, UINT alignment);



    // DXR関連オブジェクト
    ComPtr<ID3D12Device5> m_device;
    ComPtr<ID3D12GraphicsCommandList4> m_commandList;
    ComPtr<ID3D12CommandQueue> m_commandQueue;

    // ルートシグネチャとパイプライン
    ComPtr<ID3D12RootSignature> m_globalRootSignature;
    ComPtr<ID3D12StateObject> m_rtStateObject;

    // アクセラレーション構造
    ComPtr<ID3D12Resource> m_topLevelAS;
    ComPtr<ID3D12Resource> m_topLevelASScratch;
    std::vector<ComPtr<ID3D12Resource>> m_bottomLevelAS;
    std::vector<ComPtr<ID3D12Resource>> m_bottomLevelASScratch;

    // シェーダーテーブル
    ComPtr<ID3D12Resource> m_rayGenShaderTable;
    ComPtr<ID3D12Resource> m_missShaderTable;
    ComPtr<ID3D12Resource> m_hitGroupShaderTable;

    // DXCコンパイラーインスタンス
    ComPtr<IDxcUtils> m_dxcUtils;
    ComPtr<IDxcCompiler3> m_dxcCompiler;
    ComPtr<IDxcIncludeHandler> m_includeHandler;

    // 出力リソース
    ComPtr<ID3D12Resource> m_raytracingOutput;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;

    // デノイザー用リソース
    ComPtr<ID3D12PipelineState> m_denoiserPSO;
    ComPtr<ID3D12RootSignature> m_denoiserRootSignature;
    ComPtr<ID3D12Resource> m_denoiserConstants;
    ComPtr<ID3D12DescriptorHeap> m_denoiserDescriptorHeap;
    // G-Buffer用テクスチャ(デノイザー用)
    ComPtr<ID3D12Resource> m_albedoBuffer;
    ComPtr<ID3D12Resource> m_normalBuffer;
    ComPtr<ID3D12Resource> m_depthBuffer;
    ComPtr<ID3D12Resource> m_denoisedOutput;
    
    // **時間的蓄積用テクスチャ**
    ComPtr<ID3D12Resource> m_accumulationBuffer;   // u1: 蓄積バッファ
    ComPtr<ID3D12Resource> m_prevFrameDataBuffer;  // u2: 前フレームデータ
    bool m_temporalAccumulationInitialized = false; // 初期化フラグ
    
    // **ReSTIR DI用Reservoirバッファ**
    ComPtr<ID3D12Resource> m_currentReservoirs;    // u6: 現在フレームのReservoir
    ComPtr<ID3D12Resource> m_previousReservoirs;   // u7: 前フレームのReservoir
    ComPtr<ID3D12Resource> m_restirUploadBuffer;   // ReSTIR初期化用アップロードバッファ
    bool m_restirInitialized = false;              // ReSTIR初期化フラグ
    
    // **ReSTIR GI用Reservoirバッファ（将来実装用）**
    ComPtr<ID3D12Resource> m_currentGIReservoirs;  // u8: 現在フレームのGI Reservoir
    ComPtr<ID3D12Resource> m_previousGIReservoirs; // u9: 前フレームのGI Reservoir
    ComPtr<ID3D12Resource> m_giUploadBuffer;       // ReSTIR GI初期化用アップロードバッファ
    bool m_restirGIInitialized = false;            // ReSTIR GI初期化フラグ

    // デノイザー定数構造体
    struct DenoiserConstants {
        int stepSize;
        float colorSigma;
        float normalSigma;
        float depthSigma;
        DirectX::XMFLOAT2 texelSize;
        DirectX::XMFLOAT2 padding;
    };

    // 定数バッファ
    struct SceneConstantBuffer {
        XMMATRIX projectionMatrix;  // 64 bytes
        XMMATRIX viewMatrix;        // 64 bytes

        DirectX::XMFLOAT3 cameraRight;         // 12 bytes
        float tanHalfFov;                      // 4 bytes

        DirectX::XMFLOAT3 cameraUp;            // 12 bytes
        float aspectRatio;                     // 4 bytes

        DirectX::XMFLOAT3 cameraForward;       // 12 bytes  
        float frameCount;                      // 4 bytes
        
        uint32_t numLights;                    // 4 bytes
        uint32_t cameraMovedFlag;              // 4 bytes (カメラ動き検出フラグ)
        float padding[2];                      // 8 bytes (アライメント用)
    };
    ComPtr<ID3D12Resource> m_sceneConstantBuffer;

    // 各種バッファ
    ComPtr<ID3D12Resource> m_materialBuffer;
    ComPtr<ID3D12Resource> m_globalVertexBuffer;
    ComPtr<ID3D12Resource> m_globalIndexBuffer;
    ComPtr<ID3D12Resource> m_instanceOffsetBuffer;
    
    // ライト関連バッファー
    ComPtr<ID3D12Resource> m_lightBuffer;
    std::vector<DXRLightData> m_lightData;
    uint32_t m_numLights = 0;

    // IMGUI表示用SRVヒープ/デバッグヒープ内で順序管理に使うための開始ハンドル
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_debugSrvHeapStart_CPU;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_debugSrvHeapStart_GPU;

    UINT m_totalVertexCount = 0;
    UINT m_totalIndexCount = 0;

    // シェーダー識別子
    static const UINT s_shaderIdentifierSize = 32;
    static const UINT s_shaderTableEntrySize = 32;
    UINT s_hitGroupEntrySize = 32; // 動的に設定

    // ディメンション
    UINT m_width = 1920;
    UINT m_height = 1080;

    // デノイザー設定
    bool m_denoiserEnabled = true;
    int m_denoiserIterations = 3;
    float m_colorSigma = 0.125f;
    float m_normalSigma = 32.0f;
    float m_depthSigma = 0.1f;
};