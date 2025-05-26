#pragma once


struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};

class Scene;
class Material;


class Renderer {
public:
    void Init();
    void Cleanup();
    void Update();
    void Render();

	void WaitGPU() {
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));

        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

        m_fenceValue++;
	}


	ComPtr<ID3D12Device> GetDevice() const { return m_device; }

	

private:
	friend class Singleton<Renderer>;

    Renderer();
    ~Renderer();


    void LoadPipeline();
    void LoadAssets();

    static const uint32_t m_frameBufferCount = 2;

    // DirectX 12 オブジェクト
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Resource> m_renderTargets[m_frameBufferCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    uint32_t m_rtvDescriptorSize;
    uint32_t m_frameIndex;

    // 同期用オブジェクト
    ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue;
    HANDLE m_fenceEvent;

    // パイプラインステート
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // 頂点バッファ
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // ウィンドウのサイズ
    uint32_t m_bufferWidth;
    uint32_t m_bufferHeight;
	HWND m_hWnd;
};
