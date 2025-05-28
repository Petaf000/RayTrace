// DXRBox.h
#pragma once
#include "DXRShape.h"

class DXRBox : public DXRShape {
public:
    DXRBox() = default;
    DXRBox(const XMFLOAT3& size, const DXRMaterialData& material)
        : m_size(size) {
        m_materialData = material;
    };
    virtual ~DXRBox() = default;

    virtual void Init() override;
    virtual void UnInit() override;

    virtual std::vector<DXRVertex> GetVertices() const override { return m_vertices; };
    virtual std::vector<uint32_t> GetIndices() const override { return m_indices; };
    virtual DXRMaterialData GetMaterialData() const override { return m_materialData; };
    virtual void SetMaterialData(const DXRMaterialData& material) override { m_materialData = material; };

    void SetSize(const XMFLOAT3& size) { m_size = size; }
    XMFLOAT3 GetSize() const { return m_size; }

private:
    void CreateBoxGeometry();

    XMFLOAT3 m_size = { 1.0f, 1.0f, 1.0f };
    std::vector<DXRVertex> m_vertices;
    std::vector<uint32_t> m_indices;
};