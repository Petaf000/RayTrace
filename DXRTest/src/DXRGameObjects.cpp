#include "DXRGameObjects.h"

// DXRShape基底実装
bool DXRShape::CreateBLASBuffers(ID3D12Device5* device) {
    if ( m_vertices.empty() || m_indices.empty() ) return false;

    // 頂点バッファ作成
    const UINT vertexBufferSize = static_cast<UINT>( sizeof(DXRVertex) * m_vertices.size() );
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    HRESULT hr = device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vertexBuffer));
    if ( FAILED(hr) ) return false;

    // インデックスバッファ作成
    const UINT indexBufferSize = static_cast<UINT>( sizeof(uint32_t) * m_indices.size() );
    CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

    hr = device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &indexBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_indexBuffer));
    if ( FAILED(hr) ) return false;

    // データのアップロード（実際のプロジェクトではコマンドリストを使用）
    // ここでは簡略化のため、アップロードヒープ経由でコピー

    return true;
}

// DXRSphere実装
DXRSphere::DXRSphere(float radius, int segments)
    : m_radius(radius), m_segments(segments) {
    CreateSphereGeometry();
}

void DXRSphere::Init() {
    // 初期化処理
}

void DXRSphere::Update() {
    // 更新処理
}

void DXRSphere::Draw() {
    // 描画処理（DXRでは不要）
}

void DXRSphere::UnInit() {
    // 終了処理
}

void DXRSphere::BuildBLAS(ID3D12Device5* device) {
    CreateBLASBuffers(device);
}

void DXRSphere::CreateSphereGeometry() {
    m_vertices.clear();
    m_indices.clear();

    const float PI = 3.14159265359f;

    // 球体の頂点生成
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

            // 正規化された法線
            vertex.normal.x = sinTheta * cosPhi;
            vertex.normal.y = cosTheta;
            vertex.normal.z = sinTheta * sinPhi;

            // テクスチャ座標
            vertex.texcoord.x = static_cast<float>( lon ) / m_segments;
            vertex.texcoord.y = static_cast<float>( lat ) / m_segments;

            m_vertices.push_back(vertex);
        }
    }

    // インデックス生成
    for ( int lat = 0; lat < m_segments; lat++ ) {
        for ( int lon = 0; lon < m_segments; lon++ ) {
            int current = lat * ( m_segments + 1 ) + lon;
            int next = current + m_segments + 1;

            // 三角形1
            m_indices.push_back(current);
            m_indices.push_back(next);
            m_indices.push_back(current + 1);

            // 三角形2
            m_indices.push_back(current + 1);
            m_indices.push_back(next);
            m_indices.push_back(next + 1);
        }
    }
}

// DXRBox実装
DXRBox::DXRBox(const XMFLOAT3& size) : m_size(size) {
    CreateBoxGeometry();
}

void DXRBox::Init() {
    // 初期化処理
}

void DXRBox::Update() {
    // 更新処理
}

void DXRBox::Draw() {
    // 描画処理（DXRでは不要）
}

void DXRBox::UnInit() {
    // 終了処理
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

    // 立方体の8頂点
    XMFLOAT3 positions[8] = {
        { -x, -y, -z }, { x, -y, -z }, { x,  y, -z }, { -x,  y, -z },
        { -x, -y,  z }, { x, -y,  z }, { x,  y,  z }, { -x,  y,  z }
    };

    // 各面の法線
    XMFLOAT3 normals[6] = {
        { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 1, 0, 0 }
    };

    // 各面のテクスチャ座標
    XMFLOAT2 texCoords[4] = {
        { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 }
    };

    // 各面のインデックス
    int faceIndices[6][4] = {
        { 0, 1, 2, 3 }, // 前面
        { 4, 7, 6, 5 }, // 背面
        { 0, 4, 5, 1 }, // 底面
        { 2, 6, 7, 3 }, // 上面
        { 0, 3, 7, 4 }, // 左面
        { 1, 5, 6, 2 }  // 右面
    };

    // 各面について頂点とインデックスを生成
    for ( int face = 0; face < 6; face++ ) {
        int baseVertex = static_cast<int>( m_vertices.size() );

        // 4つの頂点を追加
        for ( int i = 0; i < 4; i++ ) {
            DXRVertex vertex;
            vertex.position = positions[faceIndices[face][i]];
            vertex.normal = normals[face];
            vertex.texcoord = texCoords[i];
            m_vertices.push_back(vertex);
        }

        // 2つの三角形のインデックスを追加
        m_indices.push_back(baseVertex + 0);
        m_indices.push_back(baseVertex + 1);
        m_indices.push_back(baseVertex + 2);

        m_indices.push_back(baseVertex + 0);
        m_indices.push_back(baseVertex + 2);
        m_indices.push_back(baseVertex + 3);
    }
}

// DXRRect実装
DXRRect::DXRRect(float x0, float x1, float y0, float y1, float k, AxisType axis)
    : m_x0(x0), m_x1(x1), m_y0(y0), m_y1(y1), m_k(k), m_axis(axis) {
    CreateRectGeometry();
}

void DXRRect::Init() {
    // 初期化処理
}

void DXRRect::Update() {
    // 更新処理
}

void DXRRect::Draw() {
    // 描画処理（DXRでは不要）
}

void DXRRect::UnInit() {
    // 終了処理
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
            // XY平面上の矩形
            vertices[0] = { {m_x0, m_y0, m_k}, {0, 0, 1}, {0, 0} };
            vertices[1] = { {m_x1, m_y0, m_k}, {0, 0, 1}, {1, 0} };
            vertices[2] = { {m_x1, m_y1, m_k}, {0, 0, 1}, {1, 1} };
            vertices[3] = { {m_x0, m_y1, m_k}, {0, 0, 1}, {0, 1} };
            break;

        case kXZ:
            // XZ平面上の矩形
            vertices[0] = { {m_x0, m_k, m_y0}, {0, 1, 0}, {0, 0} };
            vertices[1] = { {m_x1, m_k, m_y0}, {0, 1, 0}, {1, 0} };
            vertices[2] = { {m_x1, m_k, m_y1}, {0, 1, 0}, {1, 1} };
            vertices[3] = { {m_x0, m_k, m_y1}, {0, 1, 0}, {0, 1} };
            break;

        case kYZ:
            // YZ平面上の矩形
            vertices[0] = { {m_k, m_x0, m_y0}, {1, 0, 0}, {0, 0} };
            vertices[1] = { {m_k, m_x1, m_y0}, {1, 0, 0}, {1, 0} };
            vertices[2] = { {m_k, m_x1, m_y1}, {1, 0, 0}, {1, 1} };
            vertices[3] = { {m_k, m_x0, m_y1}, {1, 0, 0}, {0, 1} };
            break;
    }

    // 頂点追加
    for ( int i = 0; i < 4; i++ ) {
        m_vertices.push_back(vertices[i]);
    }

    // インデックス追加（2つの三角形）
    m_indices.push_back(0);
    m_indices.push_back(1);
    m_indices.push_back(2);

    m_indices.push_back(0);
    m_indices.push_back(2);
    m_indices.push_back(3);
}

// DXRLight実装
DXRLight::DXRLight(float x0, float x1, float y0, float y1, float k, AxisType axis, const XMFLOAT3& emission)
    : DXRRect(x0, x1, y0, y1, k, axis), m_emission(emission) {
}

void DXRLight::Init() {
    DXRRect::Init();
    // ライト固有の初期化があればここに追加
}