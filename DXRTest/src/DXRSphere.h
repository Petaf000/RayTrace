// DXRSphere.h
#pragma once
#include "DXRShape.h"

class DXRSphere : public DXRShape {
public:
    DXRSphere() = default;
    DXRSphere(float radius, const uint32_t& material, int segments = 32, int rings = 16)
        : m_radius(radius), m_segments(segments), m_rings(rings) {
        m_Transform.Scale = radius;
        m_materialID = material;
    };
    virtual ~DXRSphere() = default;

    virtual void Init() override;
    virtual void UnInit() override;

    virtual std::vector<DXRVertex> GetVertices() const override { return m_vertices; };
    virtual std::vector<uint32_t> GetIndices() const override { return m_indices; };

	void SetScale(const XMFLOAT3& scale) override {
		// SetScaleでScaleを球に合わせるためにxのみ使用
		SetRadius(scale.x);
	}

    void SetRadius(float radius) {
		GameObject3D::SetScale(XMFLOAT3(radius, radius, radius));
        m_radius = radius;
        if ( !m_vertices.empty() ) {
            CreateSphereGeometry();
        }
    }
    float GetRadius() const { return m_radius; }

private:
    void CreateSphereGeometry();

    float m_radius = 1.0f;
    int m_segments = 32;  // 水平分割数
    int m_rings = 16;     // 垂直分割数
    std::vector<DXRVertex> m_vertices;
    std::vector<uint32_t> m_indices;
};