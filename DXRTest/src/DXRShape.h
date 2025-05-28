#pragma once
#include "GameObject3D.h"
#include "DXRData.h"

class DXRShape : public GameObject3D {
public:
    DXRShape() = default;
    virtual ~DXRShape() = default;

    // GameObject3D�̉��z�֐�����
    virtual void Init() override {}
    virtual void Update() override {}
    virtual void Draw() override {} // ���݂͋����
    virtual void UnInit() override {}

    // DXR�p�f�[�^�擾�i�������z�֐��j
    virtual std::vector<DXRVertex> GetVertices() const = 0;
    virtual std::vector<uint32_t> GetIndices() const = 0;
    virtual DXRMaterialData GetMaterialData() const = 0;
    virtual void SetMaterialData(const DXRMaterialData& material) = 0;

    // BLAS�\�z�p�f�[�^�擾
    BLASData GetBLASData() const {
        BLASData blasData;
        blasData.vertices = GetVertices();
        blasData.indices = GetIndices();
        blasData.material = GetMaterialData();
        blasData.transform = GetWorldMatrix();
        return blasData;
    }

protected:
    DXRMaterialData m_materialData;
};