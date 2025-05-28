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

    const float PI = 3.14159265359f;

    // 頂点生成
    for ( int ring = 0; ring <= m_rings; ++ring ) {
        float phi = PI * ring / m_rings;  // 0 to PI
        float y = cosf(phi);
        float ringRadius = sinf(phi);

        for ( int segment = 0; segment <= m_segments; ++segment ) {
            float theta = 2.0f * PI * segment / m_segments;  // 0 to 2PI
            float x = ringRadius * cosf(theta);
            float z = ringRadius * sinf(theta);

            DXRVertex vertex;
            vertex.position = { x * m_radius, y * m_radius, z * m_radius };
            vertex.normal = { x, y, z };
            vertex.texCoord = {
                static_cast<float>( segment ) / m_segments,
                static_cast<float>( ring ) / m_rings
            };

            m_vertices.push_back(vertex);
        }
    }

    // インデックス生成
    for ( int ring = 0; ring < m_rings; ++ring ) {
        for ( int segment = 0; segment < m_segments; ++segment ) {
            int current = ring * ( m_segments + 1 ) + segment;
            int next = current + m_segments + 1;

            // 各四角形を2つの三角形に分割
            m_indices.push_back(current);
            m_indices.push_back(next);
            m_indices.push_back(current + 1);

            m_indices.push_back(current + 1);
            m_indices.push_back(next);
            m_indices.push_back(next + 1);
        }
    }
}