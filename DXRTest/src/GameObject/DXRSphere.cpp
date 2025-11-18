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

    const int SEGMENTS = 16;
    const int RINGS = 8;
    int expectedVertices = ( RINGS + 1 ) * ( SEGMENTS + 1 );
    int expectedTriangles = RINGS * SEGMENTS * 2;
    int expectedIndices = expectedTriangles * 3;

    const float PI = 3.14159265359f;

    for ( int ring = 0; ring <= RINGS; ++ring ) {
        float phi = PI * ring / RINGS;
        float y = cosf(phi);
        float ringRadius = sinf(phi);

        for ( int segment = 0; segment <= SEGMENTS; ++segment ) {
            float theta = 2.0f * PI * segment / SEGMENTS;
            float x = ringRadius * cosf(theta);
            float z = ringRadius * sinf(theta);

            DXRVertex vertex;
            vertex.position = { x, y, z };
            vertex.normal = { x, y, z };
            vertex.texCoord = {
                static_cast<float>( segment ) / SEGMENTS,
                static_cast<float>( ring ) / RINGS
            };

            m_vertices.push_back(vertex);
        }
    }
    int triangleCount = 0;
    int verticesPerRing = SEGMENTS + 1;

    for ( int ring = 0; ring < RINGS; ++ring ) {
        for ( int segment = 0; segment < SEGMENTS; ++segment ) {
            int topLeft = ring * verticesPerRing + segment;
            int topRight = topLeft + 1;
            int bottomLeft = ( ring + 1 ) * verticesPerRing + segment;
            int bottomRight = bottomLeft + 1;

            if ( topLeft >= m_vertices.size() || topRight >= m_vertices.size() ||
                bottomLeft >= m_vertices.size() || bottomRight >= m_vertices.size() ) {
                
                continue;
            }

            m_indices.push_back(static_cast<uint32_t>( topLeft ));
            m_indices.push_back(static_cast<uint32_t>( topRight ));
            m_indices.push_back(static_cast<uint32_t>( bottomLeft ));
            triangleCount++;

            m_indices.push_back(static_cast<uint32_t>( topRight ));
            m_indices.push_back(static_cast<uint32_t>( bottomRight ));
            m_indices.push_back(static_cast<uint32_t>( bottomLeft ));
            triangleCount++;
        }
    }
    uint32_t maxIndex = 0;
    bool hasErrors = false;

    for ( size_t i = 0; i < m_indices.size(); ++i ) {
        uint32_t idx = m_indices[i];
        maxIndex = max(maxIndex, idx);

        if ( idx >= m_vertices.size() ) {
            hasErrors = true;
        }
    }
    if ( !hasErrors && maxIndex < m_vertices.size() ) {
    }
    else {
    }

    m_segments = SEGMENTS;
    m_rings = RINGS;
}