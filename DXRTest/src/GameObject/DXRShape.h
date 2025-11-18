#pragma once
#include "GameObject3D.h"
#include "System/Renderer/DXRData.h"

class DXRShape : public GameObject3D {
public:
    DXRShape() = default;
    virtual ~DXRShape() = default;

    virtual void Init() override {}
    virtual void Update() override {}
    virtual void Draw() override {}
    virtual void UnInit() override {}

    virtual std::vector<DXRVertex> GetVertices() const = 0;
    virtual std::vector<uint32_t> GetIndices() const = 0;
    virtual uint32_t GetMaterialID() const { return m_materialID; }
    virtual void SetMaterialID(uint32_t id) { m_materialID = id; }

    BLASData GetBLASData() const {
        BLASData blasData;
        blasData.vertices = GetVertices();
        blasData.indices = GetIndices();
        blasData.materialID = GetMaterialID();

        blasData.transform = GetWorldMatrix();

        return blasData;
    }

protected:
    uint32_t m_materialID;
};