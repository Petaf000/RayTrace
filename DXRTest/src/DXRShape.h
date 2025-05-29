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
    virtual DXRMaterialData GetMaterialData() const = 0;
    virtual void SetMaterialData(const DXRMaterialData& material) = 0;

    // BLAS構築用データ取得
    BLASData GetBLASData() const {
        BLASData blasData;

        // ★修正：ローカル座標をそのまま使用（ワールド変換しない）
        auto localVertices = GetVertices();
        auto localIndices = GetIndices();

        // ★修正：頂点はローカル座標のまま使用
        blasData.vertices = localVertices;
        blasData.indices = localIndices;
        blasData.material = GetMaterialData();

        // ★修正：ワールド変換行列をtransformに設定
        blasData.transform = GetWorldMatrix();
        return blasData;
    }

protected:
    DXRMaterialData m_materialData;
};