// DXRSphere.h
#pragma once
#include "DXRShape.h"

class DXRSphere : public DXRShape {
public:
    DXRSphere() = default;
    DXRSphere(float radius, const DXRMaterialData& material, int segments = 32, int rings = 16)
        : m_radius(radius), m_segments(segments), m_rings(rings) {
        m_materialData = material;
    };
    virtual ~DXRSphere() = default;

    virtual void Init() override;
    virtual void UnInit() override;

    virtual std::vector<DXRVertex> GetVertices() const override { return m_vertices; };
    virtual std::vector<uint32_t> GetIndices() const override { return m_indices; };
    virtual DXRMaterialData GetMaterialData() const override { return m_materialData; };
    virtual void SetMaterialData(const DXRMaterialData& material) override { m_materialData = material; };

    void SetRadius(float radius) { m_radius = radius; }
    float GetRadius() const { return m_radius; }

private:
    void CreateSphereGeometry();

    float m_radius = 1.0f;
    int m_segments = 32;  // …•½•ªŠ„”
    int m_rings = 16;     // ‚’¼•ªŠ„”
    std::vector<DXRVertex> m_vertices;
    std::vector<uint32_t> m_indices;
};