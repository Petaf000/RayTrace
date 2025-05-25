#include "Sphere.h"
#include "Renderer.h"

void Sphere::Init() {
    m_Vertices.clear();
    m_Indices.clear();

    // 頂点生成
    for ( int stack = 0; stack <= m_StackCount; stack++ ) {
        float phi = XM_PI * stack / m_StackCount;
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);

        for ( int slice = 0; slice <= m_SliceCount; slice++ ) {
            float theta = 2.0f * XM_PI * slice / m_SliceCount;
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);

            XMFLOAT3 position;
            position.x = m_Center.x + m_Radius * sinPhi * cosTheta;
            position.y = m_Center.y + m_Radius * cosPhi;
            position.z = m_Center.z + m_Radius * sinPhi * sinTheta;

            m_Vertices.push_back(position);
        }
    }

    // インデックス生成
    for ( int stack = 0; stack < m_StackCount; stack++ ) {
        for ( int slice = 0; slice < m_SliceCount; slice++ ) {
            int index1 = stack * ( m_SliceCount + 1 ) + slice;
            int index2 = stack * ( m_SliceCount + 1 ) + slice + 1;
            int index3 = ( stack + 1 ) * ( m_SliceCount + 1 ) + slice;
            int index4 = ( stack + 1 ) * ( m_SliceCount + 1 ) + slice + 1;

            // 上側の三角形
            m_Indices.push_back(index1);
            m_Indices.push_back(index3);
            m_Indices.push_back(index2);

            // 下側の三角形
            m_Indices.push_back(index2);
            m_Indices.push_back(index3);
            m_Indices.push_back(index4);
        }
    }

    // 頂点バッファの作成
    UINT vertexBufferSize = static_cast<UINT>( m_Vertices.size() * sizeof(XMFLOAT3) );

    // ヒープ設定
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    // リソース設定
    D3D12_RESOURCE_DESC vertexBufferDesc = {};
    vertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexBufferDesc.Width = vertexBufferSize;
    vertexBufferDesc.Height = 1;
    vertexBufferDesc.DepthOrArraySize = 1;
    vertexBufferDesc.MipLevels = 1;
    vertexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    vertexBufferDesc.SampleDesc.Count = 1;
    vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 頂点バッファの作成
    ID3D12Device* device = Singleton<Renderer>::getInstance().GetDevice().Get();

    device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_VertexBuffer));

    // 頂点データのコピー
    void* pData;
    m_VertexBuffer->Map(0, nullptr, &pData);
    memcpy(pData, m_Vertices.data(), vertexBufferSize);
    m_VertexBuffer->Unmap(0, nullptr);

    // インデックスバッファの作成
    UINT indexBufferSize = static_cast<UINT>( m_Indices.size() * sizeof(UINT) );

    // リソース設定
    D3D12_RESOURCE_DESC indexBufferDesc = {};
    indexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    indexBufferDesc.Width = indexBufferSize;
    indexBufferDesc.Height = 1;
    indexBufferDesc.DepthOrArraySize = 1;
    indexBufferDesc.MipLevels = 1;
    indexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    indexBufferDesc.SampleDesc.Count = 1;
    indexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &indexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_IndexBuffer));

    // インデックスデータのコピー
    m_IndexBuffer->Map(0, nullptr, &pData);
    memcpy(pData, m_Indices.data(), indexBufferSize);
    m_IndexBuffer->Unmap(0, nullptr);
}

void Sphere::UnInit() {
    m_VertexBuffer.Reset();
    m_IndexBuffer.Reset();
}

D3D12_RAYTRACING_GEOMETRY_DESC Sphere::GetGeometryDesc() {
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.VertexBuffer.StartAddress = m_VertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(XMFLOAT3);
    geometryDesc.Triangles.VertexCount = static_cast<UINT>( m_Vertices.size() );
    geometryDesc.Triangles.IndexBuffer = m_IndexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.IndexCount = static_cast<UINT>( m_Indices.size() );
    geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    return geometryDesc;
}