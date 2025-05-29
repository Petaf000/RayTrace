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

        // ���C���F���[�J�����W�����̂܂܎g�p�i���[���h�ϊ����Ȃ��j
        auto localVertices = GetVertices();
        auto localIndices = GetIndices();

        // ���C���F���_�̓��[�J�����W�̂܂܎g�p
        blasData.vertices = localVertices;
        blasData.indices = localIndices;
        blasData.material = GetMaterialData();

        // ���C���F���[���h�ϊ��s���transform�ɐݒ�
        blasData.transform = GetWorldMatrix();
        return blasData;
    }

protected:
    DXRMaterialData m_materialData;
};