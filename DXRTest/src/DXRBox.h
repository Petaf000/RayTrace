// DXRBox.h
#pragma once
#include "DXRShape.h"

class DXRBox : public DXRShape {
public:
    DXRBox() = default;
    DXRBox(const XMFLOAT3& size, const DXRMaterialData& material) {
		m_Transform.Scale = size;
        m_materialData = material;
    };
    virtual ~DXRBox() = default;

    virtual void Init() override;
    virtual void UnInit() override;

    virtual std::vector<DXRVertex> GetVertices() const override { return m_vertices; };
    virtual std::vector<uint32_t> GetIndices() const override { return m_indices; };
    virtual DXRMaterialData GetMaterialData() const override { return m_materialData; };
    virtual void SetMaterialData(const DXRMaterialData& material) override { m_materialData = material; };

    // ★修正：SetSize()でジオメトリを再生成するように変更
    void SetScale(const XMFLOAT3& scale) override{
		GameObject3D::SetScale(scale);
        // サイズが変更されたらジオメトリを再生成
        if ( !m_vertices.empty() ) {
            CreateBoxGeometry();
        }
    }

private:
    void CreateBoxGeometry();
    std::vector<DXRVertex> m_vertices;
    std::vector<uint32_t> m_indices;
};