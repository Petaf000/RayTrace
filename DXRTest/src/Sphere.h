#pragma once
#include "Shape.h"

// 球形状クラス
class Sphere : public Shape {
public:
    Sphere(const XMFLOAT3& center, float radius, int sliceCount = 32, int stackCount = 16)
        : m_Center(center), m_Radius(radius), m_SliceCount(sliceCount), m_StackCount(stackCount) {
    }

    virtual ~Sphere() override = default;

    // 初期化
    void Init() override;

    // 解放
    void UnInit() override;

    // ジオメトリ記述を取得
    D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc() override;

private:
    XMFLOAT3 m_Center;                        // 中心
    float m_Radius;                           // 半径
    int m_SliceCount;                         // 水平方向の分割数
    int m_StackCount;                         // 垂直方向の分割数
    std::vector<XMFLOAT3> m_Vertices;         // 頂点データ
    std::vector<UINT> m_Indices;              // インデックスデータ
    ComPtr<ID3D12Resource> m_VertexBuffer;    // 頂点バッファ
    ComPtr<ID3D12Resource> m_IndexBuffer;     // インデックスバッファ
};