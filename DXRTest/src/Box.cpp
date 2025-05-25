#include "Box.h"
#include "Renderer.h"

void Box::Init() {
    // 8つの頂点を作成
    XMFLOAT3 vertices[8] = {
        XMFLOAT3(m_Min.x, m_Min.y, m_Min.z), // 0: 左下手前
        XMFLOAT3(m_Max.x, m_Min.y, m_Min.z), // 1: 右下手前
        XMFLOAT3(m_Min.x, m_Max.y, m_Min.z), // 2: 左上手前
        XMFLOAT3(m_Max.x, m_Max.y, m_Min.z), // 3: 右上手前
        XMFLOAT3(m_Min.x, m_Min.y, m_Max.z), // 4: 左下奥
        XMFLOAT3(m_Max.x, m_Min.y, m_Max.z), // 5: 右下奥
        XMFLOAT3(m_Min.x, m_Max.y, m_Max.z), // 6: 左上奥
        XMFLOAT3(m_Max.x, m_Max.y, m_Max.z)  // 7: 右上奥
    };

    // 12個の三角形を作成（6つの面、各面2つの三角形）
    m_Triangles.clear();

    // 前面（-Z）
    m_Triangles.push_back(Triangle(vertices[0], vertices[1], vertices[2]));
    m_Triangles.push_back(Triangle(vertices[1], vertices[3], vertices[2]));

    // 背面（+Z）
    m_Triangles.push_back(Triangle(vertices[5], vertices[4], vertices[7]));
    m_Triangles.push_back(Triangle(vertices[4], vertices[6], vertices[7]));

    // 左面（-X）
    m_Triangles.push_back(Triangle(vertices[4], vertices[0], vertices[6]));
    m_Triangles.push_back(Triangle(vertices[0], vertices[2], vertices[6]));

    // 右面（+X）
    m_Triangles.push_back(Triangle(vertices[1], vertices[5], vertices[3]));
    m_Triangles.push_back(Triangle(vertices[5], vertices[7], vertices[3]));

    // 下面（-Y）
    m_Triangles.push_back(Triangle(vertices[0], vertices[4], vertices[1]));
    m_Triangles.push_back(Triangle(vertices[4], vertices[5], vertices[1]));

    // 上面（+Y）
    m_Triangles.push_back(Triangle(vertices[2], vertices[3], vertices[6]));
    m_Triangles.push_back(Triangle(vertices[3], vertices[7], vertices[6]));

    // 頂点バッファの作成
    std::vector<XMFLOAT3> vertexList;
    for ( const auto& triangle : m_Triangles ) {
        vertexList.push_back(triangle.vertices[0]);
        vertexList.push_back(triangle.vertices[1]);
        vertexList.push_back(triangle.vertices[2]);
    }

    UINT vertexBufferSize = static_cast<UINT>( vertexList.size() * sizeof(XMFLOAT3) );

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
    memcpy(pData, vertexList.data(), vertexBufferSize);
    m_VertexBuffer->Unmap(0, nullptr);
}

void Box::UnInit() {
    m_VertexBuffer.Reset();
    m_IndexBuffer.Reset();
}

D3D12_RAYTRACING_GEOMETRY_DESC Box::GetGeometryDesc() {
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.VertexBuffer.StartAddress = m_VertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(XMFLOAT3);
    geometryDesc.Triangles.VertexCount = static_cast<UINT>( m_Triangles.size() * 3 );
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    return geometryDesc;
}