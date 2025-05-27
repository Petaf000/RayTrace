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


    // �ŏ����̒ǉ����\�b�h
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    HWND GetHwnd() const { return m_hWnd; }
    UINT GetBufferWidth() const { return m_bufferWidth; }
    UINT GetBufferHeight() const { return m_bufferHeight; }

    bool CheckDXRSupport() const;
    ID3D12Device5* GetDXRDevice() const;

private:
	friend class Singleton<Renderer>;

    Renderer();
    ~Renderer();


    void LoadPipeline();
    void LoadAssets();

    static const uint32_t m_frameBufferCount = 2;

    // DirectX 12 �I�u�W�F�N�g
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

    // �����p�I�u�W�F�N�g
    ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue;
    HANDLE m_fenceEvent;

    // �p�C�v���C���X�e�[�g
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    // ���_�o�b�t�@
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // �E�B���h�E�̃T�C�Y
    uint32_t m_bufferWidth;
    uint32_t m_bufferHeight;
	HWND m_hWnd;


    // �����̃����o�[�ϐ��ɉ����āA����炪�K�v
    mutable ComPtr<ID3D12Device5> m_dxrDevice; // �L���b�V���p
};
