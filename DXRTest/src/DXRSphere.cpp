// DXRSphere.cpp
#include "DXRSphere.h"

void DXRSphere::Init() {
    CreateSphereGeometry();
}

void DXRSphere::UnInit() {
    m_vertices.clear();
    m_indices.clear();
}

void DXRSphere::CreateSphereGeometry() {
    m_vertices.clear();
    m_indices.clear();

    char debugMsg[256];
    sprintf_s(debugMsg, "Creating Sphere Geometry\n");
    OutputDebugStringA(debugMsg);

    // 球らしく見える適切な分割数
    const int SEGMENTS = 16;  // 水平分割数（経度方向）
    const int RINGS = 8;      // 垂直分割数（緯度方向）

    sprintf_s(debugMsg, "Using values: Segments=%d, Rings=%d\n", SEGMENTS, RINGS);
    OutputDebugStringA(debugMsg);

    // 期待される数値を計算
    int expectedVertices = ( RINGS + 1 ) * ( SEGMENTS + 1 );  // = 9 * 17 = 153
    int expectedTriangles = RINGS * SEGMENTS * 2;         // = 8 * 16 * 2 = 256
    int expectedIndices = expectedTriangles * 3;          // = 256 * 3 = 768

    sprintf_s(debugMsg, "Expected: %d vertices, %d triangles, %d indices\n",
        expectedVertices, expectedTriangles, expectedIndices);
    OutputDebugStringA(debugMsg);

    const float PI = 3.14159265359f;

    // 頂点生成
    for ( int ring = 0; ring <= RINGS; ++ring ) {
        float phi = PI * ring / RINGS;  // 0 から π まで
        float y = cosf(phi);            // Y座標（上から下へ）
        float ringRadius = sinf(phi);   // この緯度での半径

        for ( int segment = 0; segment <= SEGMENTS; ++segment ) {
            float theta = 2.0f * PI * segment / SEGMENTS;  // 0 から 2π まで
            float x = ringRadius * cosf(theta);
            float z = ringRadius * sinf(theta);

            DXRVertex vertex;
            vertex.position = { x, y, z };
            vertex.normal = { x, y, z };  // 球の法線は位置ベクトルと同じ
            vertex.texCoord = {
                static_cast<float>( segment ) / SEGMENTS,
                static_cast<float>( ring ) / RINGS
            };

            m_vertices.push_back(vertex);
        }
    }

    sprintf_s(debugMsg, "Generated %zu vertices (expected: %d)\n", m_vertices.size(), expectedVertices);
    OutputDebugStringA(debugMsg);

    // インデックス生成
    int triangleCount = 0;
    int verticesPerRing = SEGMENTS + 1;

    sprintf_s(debugMsg, "Vertices per ring: %d\n", verticesPerRing);
    OutputDebugStringA(debugMsg);

    for ( int ring = 0; ring < RINGS; ++ring ) {
        for ( int segment = 0; segment < SEGMENTS; ++segment ) {
            // 四角形の4つの頂点のインデックス
            int topLeft = ring * verticesPerRing + segment;
            int topRight = topLeft + 1;
            int bottomLeft = ( ring + 1 ) * verticesPerRing + segment;
            int bottomRight = bottomLeft + 1;

            // 範囲チェック
            if ( topLeft >= m_vertices.size() || topRight >= m_vertices.size() ||
                bottomLeft >= m_vertices.size() || bottomRight >= m_vertices.size() ) {
                sprintf_s(debugMsg, "ERROR: Index out of range! vertices=%zu\n", m_vertices.size());
                OutputDebugStringA(debugMsg);
                continue;
            }

            // 三角形1: (topLeft, topRight, bottomLeft)
            m_indices.push_back(static_cast<uint32_t>( topLeft ));
            m_indices.push_back(static_cast<uint32_t>( topRight ));
            m_indices.push_back(static_cast<uint32_t>( bottomLeft ));
            triangleCount++;

            // 三角形2: (topRight, bottomRight, bottomLeft)
            m_indices.push_back(static_cast<uint32_t>( topRight ));
            m_indices.push_back(static_cast<uint32_t>( bottomRight ));
            m_indices.push_back(static_cast<uint32_t>( bottomLeft ));
            triangleCount++;
        }
    }

    sprintf_s(debugMsg, "Generated %zu indices, %d triangles (expected: %d indices)\n",
        m_indices.size(), triangleCount, expectedIndices);
    OutputDebugStringA(debugMsg);

    // 基本的な検証
    uint32_t maxIndex = 0;
    bool hasErrors = false;

    for ( size_t i = 0; i < m_indices.size(); ++i ) {
        uint32_t idx = m_indices[i];
        maxIndex = max(maxIndex, idx);

        if ( idx >= m_vertices.size() ) {
            sprintf_s(debugMsg, "ERROR: Index %u >= vertices %zu at position %zu\n",
                idx, m_vertices.size(), i);
            OutputDebugStringA(debugMsg);
            hasErrors = true;
        }
    }

    sprintf_s(debugMsg, "Max index: %u, Vertices: %zu\n", maxIndex, m_vertices.size());
    OutputDebugStringA(debugMsg);

    if ( !hasErrors && maxIndex < m_vertices.size() ) {
        OutputDebugStringA("Sphere creation SUCCESS!\n");
        sprintf_s(debugMsg, "Sphere stats: %zu vertices, %d triangles\n",
            m_vertices.size(), triangleCount);
        OutputDebugStringA(debugMsg);
    }
    else {
        OutputDebugStringA("Sphere creation FAILED!\n");
    }

    // メンバー変数を更新
    m_segments = SEGMENTS;
    m_rings = RINGS;
}