#include "Box.h"
#include "Renderer.h"

void Box::Init() {
    // 8�̒��_���쐬
    XMFLOAT3 vertices[8] = {
        XMFLOAT3(m_Min.x, m_Min.y, m_Min.z), // 0: ������O
        XMFLOAT3(m_Max.x, m_Min.y, m_Min.z), // 1: �E����O
        XMFLOAT3(m_Min.x, m_Max.y, m_Min.z), // 2: �����O
        XMFLOAT3(m_Max.x, m_Max.y, m_Min.z), // 3: �E���O
        XMFLOAT3(m_Min.x, m_Min.y, m_Max.z), // 4: ������
        XMFLOAT3(m_Max.x, m_Min.y, m_Max.z), // 5: �E����
        XMFLOAT3(m_Min.x, m_Max.y, m_Max.z), // 6: ���㉜
        XMFLOAT3(m_Max.x, m_Max.y, m_Max.z)  // 7: �E�㉜
    };

    // 12�̎O�p�`���쐬�i6�̖ʁA�e��2�̎O�p�`�j
    m_Triangles.clear();

    // �O�ʁi-Z�j
    m_Triangles.push_back(Triangle(vertices[0], vertices[1], vertices[2]));
    m_Triangles.push_back(Triangle(vertices[1], vertices[3], vertices[2]));

    // �w�ʁi+Z�j
    m_Triangles.push_back(Triangle(vertices[5], vertices[4], vertices[7]));
    m_Triangles.push_back(Triangle(vertices[4], vertices[6], vertices[7]));

    // ���ʁi-X�j
    m_Triangles.push_back(Triangle(vertices[4], vertices[0], vertices[6]));
    m_Triangles.push_back(Triangle(vertices[0], vertices[2], vertices[6]));

    // �E�ʁi+X�j
    m_Triangles.push_back(Triangle(vertices[1], vertices[5], vertices[3]));
    m_Triangles.push_back(Triangle(vertices[5], vertices[7], vertices[3]));

    // ���ʁi-Y�j
    m_Triangles.push_back(Triangle(vertices[0], vertices[4], vertices[1]));
    m_Triangles.push_back(Triangle(vertices[4], vertices[5], vertices[1]));

    // ��ʁi+Y�j
    m_Triangles.push_back(Triangle(vertices[2], vertices[3], vertices[6]));
    m_Triangles.push_back(Triangle(vertices[3], vertices[7], vertices[6]));

    // ���_�o�b�t�@�̍쐬
    std::vector<XMFLOAT3> vertexList;
    for ( const auto& triangle : m_Triangles ) {
        vertexList.push_back(triangle.vertices[0]);
        vertexList.push_back(triangle.vertices[1]);
        vertexList.push_back(triangle.vertices[2]);
    }

    UINT vertexBufferSize = static_cast<UINT>( vertexList.size() * sizeof(XMFLOAT3) );

    // �q�[�v�ݒ�
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    // ���\�[�X�ݒ�
    D3D12_RESOURCE_DESC vertexBufferDesc = {};
    vertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexBufferDesc.Width = vertexBufferSize;
    vertexBufferDesc.Height = 1;
    vertexBufferDesc.DepthOrArraySize = 1;
    vertexBufferDesc.MipLevels = 1;
    vertexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    vertexBufferDesc.SampleDesc.Count = 1;
    vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // ���_�o�b�t�@�̍쐬
    
    ID3D12Device* device = Singleton<Renderer>::getInstance().GetDevice().Get();

    device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_VertexBuffer));

    // ���_�f�[�^�̃R�s�[
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