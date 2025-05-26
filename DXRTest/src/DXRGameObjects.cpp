#include "DXRGameObjects.h"

// DXRShape������
bool DXRShape::CreateBLASBuffers(ID3D12Device5* device) {
    if ( m_vertices.empty() || m_indices.empty() ) return false;

    // ���_�o�b�t�@�쐬
    const UINT vertexBufferSize = static_cast<UINT>( sizeof(DXRVertex) * m_vertices.size() );
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    HRESULT hr = device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vertexBuffer));
    if ( FAILED(hr) ) return false;

    // �C���f�b�N�X�o�b�t�@�쐬
    const UINT indexBufferSize = static_cast<UINT>( sizeof(uint32_t) * m_indices.size() );
    CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    hr = device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &indexBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_indexBuffer));
    if ( FAILED(hr) ) return false;

    // �f�[�^�̃A�b�v���[�h�i���ۂ̃v���W�F�N�g�ł̓R�}���h���X�g���g�p�j
    // �����ł͊ȗ����̂��߁A�A�b�v���[�h�q�[�v�o�R�ŃR�s�[

    return true;
}

// DXRSphere����
DXRSphere::DXRSphere(float radius, int segments)
    : m_radius(radius), m_segments(segments) {
    CreateSphereGeometry();
}

void DXRSphere::Init() {
    // ����������
}

void DXRSphere::Update() {
    // �X�V����
}

void DXRSphere::Draw() {
    // �`�揈���iDXR�ł͕s�v�j
}

void DXRSphere::UnInit() {
    // �I������
}

void DXRSphere::BuildBLAS(ID3D12Device5* device) {
    CreateBLASBuffers(device);
}

void DXRSphere::CreateSphereGeometry() {
    m_vertices.clear();
    m_indices.clear();

    const float PI = 3.14159265359f;

    // ���̂̒��_����
    for ( int lat = 0; lat <= m_segments; lat++ ) {
        float theta = lat * PI / m_segments;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        for ( int lon = 0; lon <= m_segments; lon++ ) {
            float phi = lon * 2 * PI / m_segments;
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            DXRVertex vertex;
            vertex.position.x = m_radius * sinTheta * cosPhi;
            vertex.position.y = m_radius * cosTheta;
            vertex.position.z = m_radius * sinTheta * sinPhi;

            // ���K�����ꂽ�@��
            vertex.normal.x = sinTheta * cosPhi;
            vertex.normal.y = cosTheta;
            vertex.normal.z = sinTheta * sinPhi;

            // �e�N�X�`�����W
            vertex.texcoord.x = static_cast<float>( lon ) / m_segments;
            vertex.texcoord.y = static_cast<float>( lat ) / m_segments;

            m_vertices.push_back(vertex);
        }
    }

    // �C���f�b�N�X����
    for ( int lat = 0; lat < m_segments; lat++ ) {
        for ( int lon = 0; lon < m_segments; lon++ ) {
            int current = lat * ( m_segments + 1 ) + lon;
            int next = current + m_segments + 1;

            // �O�p�`1
            m_indices.push_back(current);
            m_indices.push_back(next);
            m_indices.push_back(current + 1);

            // �O�p�`2
            m_indices.push_back(current + 1);
            m_indices.push_back(next);
            m_indices.push_back(next + 1);
        }
    }
}

// DXRBox����
DXRBox::DXRBox(const XMFLOAT3& size) : m_size(size) {
    CreateBoxGeometry();
}

void DXRBox::Init() {
    // ����������
}

void DXRBox::Update() {
    // �X�V����
}

void DXRBox::Draw() {
    // �`�揈���iDXR�ł͕s�v�j
}

void DXRBox::UnInit() {
    // �I������
}

void DXRBox::BuildBLAS(ID3D12Device5* device) {
    CreateBLASBuffers(device);
}

void DXRBox::CreateBoxGeometry() {
    m_vertices.clear();
    m_indices.clear();

    float x = m_size.x * 0.5f;
    float y = m_size.y * 0.5f;
    float z = m_size.z * 0.5f;

    // �����̂�8���_
    XMFLOAT3 positions[8] = {
        { -x, -y, -z }, { x, -y, -z }, { x,  y, -z }, { -x,  y, -z },
        { -x, -y,  z }, { x, -y,  z }, { x,  y,  z }, { -x,  y,  z }
    };

    // �e�ʂ̖@��
    XMFLOAT3 normals[6] = {
        { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 1, 0, 0 }
    };

    // �e�ʂ̃e�N�X�`�����W
    XMFLOAT2 texCoords[4] = {
        { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 }
    };

    // �e�ʂ̃C���f�b�N�X
    int faceIndices[6][4] = {
        { 0, 1, 2, 3 }, // �O��
        { 4, 7, 6, 5 }, // �w��
        { 0, 4, 5, 1 }, // ���
        { 2, 6, 7, 3 }, // ���
        { 0, 3, 7, 4 }, // ����
        { 1, 5, 6, 2 }  // �E��
    };

    // �e�ʂɂ��Ē��_�ƃC���f�b�N�X�𐶐�
    for ( int face = 0; face < 6; face++ ) {
        int baseVertex = static_cast<int>( m_vertices.size() );

        // 4�̒��_��ǉ�
        for ( int i = 0; i < 4; i++ ) {
            DXRVertex vertex;
            vertex.position = positions[faceIndices[face][i]];
            vertex.normal = normals[face];
            vertex.texcoord = texCoords[i];
            m_vertices.push_back(vertex);
        }

        // 2�̎O�p�`�̃C���f�b�N�X��ǉ�
        m_indices.push_back(baseVertex + 0);
        m_indices.push_back(baseVertex + 1);
        m_indices.push_back(baseVertex + 2);

        m_indices.push_back(baseVertex + 0);
        m_indices.push_back(baseVertex + 2);
        m_indices.push_back(baseVertex + 3);
    }
}

// DXRRect����
DXRRect::DXRRect(float x0, float x1, float y0, float y1, float k, AxisType axis)
    : m_x0(x0), m_x1(x1), m_y0(y0), m_y1(y1), m_k(k), m_axis(axis) {
    CreateRectGeometry();
}

void DXRRect::Init() {
    // ����������
}

void DXRRect::Update() {
    // �X�V����
}

void DXRRect::Draw() {
    // �`�揈���iDXR�ł͕s�v�j
}

void DXRRect::UnInit() {
    // �I������
}

void DXRRect::BuildBLAS(ID3D12Device5* device) {
    CreateBLASBuffers(device);
}

void DXRRect::CreateRectGeometry() {
    m_vertices.clear();
    m_indices.clear();

    DXRVertex vertices[4];
    XMFLOAT3 normal;

    switch ( m_axis ) {
        case kXY:
            // XY���ʏ�̋�`
            vertices[0] = { {m_x0, m_y0, m_k}, {0, 0, 1}, {0, 0} };
            vertices[1] = { {m_x1, m_y0, m_k}, {0, 0, 1}, {1, 0} };
            vertices[2] = { {m_x1, m_y1, m_k}, {0, 0, 1}, {1, 1} };
            vertices[3] = { {m_x0, m_y1, m_k}, {0, 0, 1}, {0, 1} };
            break;

        case kXZ:
            // XZ���ʏ�̋�`
            vertices[0] = { {m_x0, m_k, m_y0}, {0, 1, 0}, {0, 0} };
            vertices[1] = { {m_x1, m_k, m_y0}, {0, 1, 0}, {1, 0} };
            vertices[2] = { {m_x1, m_k, m_y1}, {0, 1, 0}, {1, 1} };
            vertices[3] = { {m_x0, m_k, m_y1}, {0, 1, 0}, {0, 1} };
            break;

        case kYZ:
            // YZ���ʏ�̋�`
            vertices[0] = { {m_k, m_x0, m_y0}, {1, 0, 0}, {0, 0} };
            vertices[1] = { {m_k, m_x1, m_y0}, {1, 0, 0}, {1, 0} };
            vertices[2] = { {m_k, m_x1, m_y1}, {1, 0, 0}, {1, 1} };
            vertices[3] = { {m_k, m_x0, m_y1}, {1, 0, 0}, {0, 1} };
            break;
    }

    // ���_�ǉ�
    for ( int i = 0; i < 4; i++ ) {
        m_vertices.push_back(vertices[i]);
    }

    // �C���f�b�N�X�ǉ��i2�̎O�p�`�j
    m_indices.push_back(0);
    m_indices.push_back(1);
    m_indices.push_back(2);

    m_indices.push_back(0);
    m_indices.push_back(2);
    m_indices.push_back(3);
}

// DXRLight����
DXRLight::DXRLight(float x0, float x1, float y0, float y1, float k, AxisType axis, const XMFLOAT3& emission)
    : DXRRect(x0, x1, y0, y1, k, axis), m_emission(emission) {
}

void DXRLight::Init() {
    DXRRect::Init();
    // ���C�g�ŗL�̏�����������΂����ɒǉ�
}