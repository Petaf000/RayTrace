#pragma once
#include "Shape.h"
#include "Triangle.h"

// ボックス形状クラス
class Box : public Shape {
public:
    Box(const XMFLOAT3& min, const XMFLOAT3& max) : m_Min(min), m_Max(max) {}
    virtual ~Box() override = default;

    // 初期化
    void Init() override;

    // 解放
    void UnInit() override;

    // ジオメトリ記述を取得
    D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc() override;

private:
    XMFLOAT3 m_Min;                           // 最小点
    XMFLOAT3 m_Max;                           // 最大点
    std::vector<Triangle> m_Triangles;        // 三角形リスト
    ComPtr<ID3D12Resource> m_VertexBuffer;    // 頂点バッファ
    ComPtr<ID3D12Resource> m_IndexBuffer;     // インデックスバッファ
};