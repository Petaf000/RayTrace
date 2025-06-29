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
    // ������ �}�e���A��ID�������悤�ɕύX ������
    virtual uint32_t GetMaterialID() const { return m_materialID; }
    virtual void SetMaterialID(uint32_t id) { m_materialID = id; }

    // BLAS�\�z�p�f�[�^�擾
    BLASData GetBLASData() const {
        BLASData blasData;
        // ������ ���_�̓��[�J�����W�̂܂܊i�[ ������
        blasData.vertices = GetVertices();
        blasData.indices = GetIndices();
        blasData.materialID = GetMaterialID();

        // ������ ���[���h�ϊ��s���transform�ɐݒ� ������
        blasData.transform = GetWorldMatrix();

        // �f�o�b�O�F�ϊ��s��̊m�F
        XMFLOAT4X4 transformFloat;
        XMStoreFloat4x4(&transformFloat, blasData.transform);
        char debugMsg[256];
        sprintf_s(debugMsg, "BLASData transform scale: (%.3f, %.3f, %.3f)\n",
            transformFloat._11, transformFloat._22, transformFloat._33);
        OutputDebugStringA(debugMsg);
        sprintf_s(debugMsg, "BLASData transform translation: (%.3f, %.3f, %.3f)\n",
            transformFloat._41, transformFloat._42, transformFloat._43);
        OutputDebugStringA(debugMsg);

        return blasData;
    }

protected:
    uint32_t m_materialID;
};