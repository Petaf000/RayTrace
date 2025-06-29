#pragma once
#include "GameObject3D.h"
#include "DXRData.h"

class DXRShape : public GameObject3D {
public:
    DXRShape() = default;
    virtual ~DXRShape() = default;

    // GameObject3Dの仮想関数実装
    virtual void Init() override {}
    virtual void Update() override {}
    virtual void Draw() override {} // 現在は空実装
    virtual void UnInit() override {}

    // DXR用データ取得（純粋仮想関数）
    virtual std::vector<DXRVertex> GetVertices() const = 0;
    virtual std::vector<uint32_t> GetIndices() const = 0;
    // ★★★ マテリアルIDを扱うように変更 ★★★
    virtual uint32_t GetMaterialID() const { return m_materialID; }
    virtual void SetMaterialID(uint32_t id) { m_materialID = id; }

    // BLAS構築用データ取得
    BLASData GetBLASData() const {
        BLASData blasData;
        // ∴∴∴ 頂点はローカル座標のまま格納 ∴∴∴
        blasData.vertices = GetVertices();
        blasData.indices = GetIndices();
        blasData.materialID = GetMaterialID();

        // ∴∴∴ ワールド変換行列をtransformに設定 ∴∴∴
        blasData.transform = GetWorldMatrix();

        // デバッグ：変換行列の確認
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