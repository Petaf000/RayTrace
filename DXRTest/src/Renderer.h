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
    void InitFrame();
    void InitFrameForDXR();
	void EndFrame();

	void WaitGPU() {
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));

        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

        m_fenceValue++;
	}

    void ResetCommandList() {
        WaitGPU();  // GPUの処理が完了するまで待機
        // コマンドリストのリセット
        ThrowIfFailed(m_commandAllocator->Reset());

        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));  // パイプラインをnullptrに設定
    }

	void ExecuteCommandListAndWait() {
		ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        WaitGPU();
	}

    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() { return m_commandList.Get(); }
    HWND GetHwnd() const { return m_hWnd; }
    UINT GetBufferWidth() const { return m_bufferWidth; }
    UINT GetBufferHeight() const { return m_bufferHeight; }
    UINT GetCurrentFrameIndex() const { return m_frameIndex; }
    ID3D12Resource* GetBackBuffer(UINT index) const {
        return ( index < m_frameBufferCount ) ? m_renderTargets[index].Get() : nullptr;
    }
    ID3D12DescriptorHeap* GetRTVHeap() const { return m_rtvHeap.Get(); }
    UINT GetRTVDescriptorSize() const { return m_rtvDescriptorSize; }
    ID3D12DescriptorHeap* GetSRVHeap() const { return m_srvHeap.Get(); }

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
