#include "Plane.h"
#include "Renderer.h"

void Plane::Init() {
    // �@���x�N�g������݂��ɐ�����2�̃x�N�g�����쐬
    XMVECTOR normalVec = XMLoadFloat3(&m_Normal);
    normalVec = XMVector3Normalize(normalVec);

    // �C�ӂ̃x�N�g���Ɩ@���Ƃ̊O�ςŐڃx�N�g�����쐬
    XMVECTOR tangentVec;
    if ( fabs(XMVectorGetX(normalVec)) < 0.9f ) {
        tangentVec = XMVector3Normalize(XMVector3Cross(normalVec, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)));
    }
    else {
        tangentVec = XMVector3Normalize(XMVector3Cross(normalVec, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
    }

    // �ڃx�N�g���Ɩ@���̊O�ςł�����̐ڃx�N�g�����쐬
    XMVECTOR bitangentVec = XMVector3Normalize(XMVector3Cross(normalVec, tangentVec));

    // �X�P�[�����O
    tangentVec = XMVectorScale(tangentVec, m_Width * 0.5f);
    bitangentVec = XMVectorScale(bitangentVec, m_Height * 0.5f);

    // ���ʂ�4�̒��_���v�Z
    XMVECTOR originVec = XMLoadFloat3(&m_Origin);
    XMVECTOR v0 = XMVectorSubtract(XMVectorSubtract(originVec, tangentVec), bitangentVec);
    XMVECTOR v1 = XMVectorAdd(XMVectorSubtract(originVec, tangentVec), bitangentVec);
    XMVECTOR v2 = XMVectorAdd(XMVectorAdd(originVec, tangentVec), bitangentVec);
    XMVECTOR v3 = XMVectorAdd(XMVectorSubtract(originVec, bitangentVec), tangentVec);

    // XMFLOAT3�ɕϊ�
    XMFLOAT3 vertices[4];
    XMStoreFloat3(&vertices[0], v0);
    XMStoreFloat3(&vertices[1], v1);
    XMStoreFloat3(&vertices[2], v2);
    XMStoreFloat3(&vertices[3], v3);

    // 2�̎O�p�`���쐬
    m_Triangles.clear();
    m_Triangles.push_back(Triangle(vertices[0], vertices[1], vertices[2]));
    m_Triangles.push_back(Triangle(vertices[0], vertices[2], vertices[3]));

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

void Plane::UnInit() {
    m_VertexBuffer.Reset();
    m_IndexBuffer.Reset();
}

D3D12_RAYTRACING_GEOMETRY_DESC Plane::GetGeometryDesc() {
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDesc.Triangles.VertexBuffer.StartAddress = m_VertexBuffer->GetGPUVirtualAddress();
    geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(XMFLOAT3);
    geometryDesc.Triangles.VertexCount = static_cast<UINT>( m_Triangles.size() * 3 );
    geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    return geometryDesc;
}