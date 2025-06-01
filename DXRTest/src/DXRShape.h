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

        // ∴∴∴ 重要：ローカル座標系の頂点をそのまま使用 ∴∴∴
        // 頂点データは単位サイズ（正規化された座標）で取得
        // 実際のスケール・位置・回転はワールド変換行列で適用
        auto localVertices = GetVertices();
        auto localIndices = GetIndices();

        // デバッグ：頂点データの範囲をチェック
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

        // ∴∴∴ 頂点はローカル座標のまま格納 ∴∴∴
        blasData.vertices = localVertices;
        blasData.indices = localIndices;
        blasData.material = GetMaterialData();

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
    DXRMaterialData m_materialData;
};