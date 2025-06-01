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

        // ������ �d�v�F���[�J�����W�n�̒��_�����̂܂܎g�p ������
        // ���_�f�[�^�͒P�ʃT�C�Y�i���K�����ꂽ���W�j�Ŏ擾
        // ���ۂ̃X�P�[���E�ʒu�E��]�̓��[���h�ϊ��s��œK�p
        auto localVertices = GetVertices();
        auto localIndices = GetIndices();

        // �f�o�b�O�F���_�f�[�^�͈̔͂��`�F�b�N
        if ( !localVertices.empty() ) {
            float minX = localVertices[0].position.x, maxX = localVertices[0].position.x;
            float minY = localVertices[0].position.y, maxY = localVertices[0].position.y;
            float minZ = localVertices[0].position.z, maxZ = localVertices[0].position.z;

            for ( const auto& vertex : localVertices ) {
                minX = min(minX, vertex.position.x); maxX = max(maxX, vertex.position.x);
                minY = min(minY, vertex.position.y); maxY = max(maxY, vertex.position.y);
                minZ = min(minZ, vertex.position.z); maxZ = max(maxZ, vertex.position.z);
            }

            char debugMsg[256];
            sprintf_s(debugMsg, "BLASData bounds: X[%.3f,%.3f] Y[%.3f,%.3f] Z[%.3f,%.3f]\n",
                minX, maxX, minY, maxY, minZ, maxZ);
            OutputDebugStringA(debugMsg);
        }

        // ������ ���_�̓��[�J�����W�̂܂܊i�[ ������
        blasData.vertices = localVertices;
        blasData.indices = localIndices;
        blasData.material = GetMaterialData();

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
    DXRMaterialData m_materialData;
};