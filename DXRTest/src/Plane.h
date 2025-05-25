#pragma once
#include "Shape.h"
#include "Triangle.h"

// 平面形状クラス
class Plane : public Shape {
public:
    Plane(const XMFLOAT3& origin, const XMFLOAT3& normal, float width, float height)
        : m_Origin(origin), m_Normal(normal), m_Width(width), m_Height(height) {
    }

    virtual ~Plane() override = default;

    // 初期化
    void Init() override;

    // 解放
    void UnInit() override;

    // ジオメトリ記述を取得
    D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc() override;

private:
    XMFLOAT3 m_Origin;                        // 原点
    XMFLOAT3 m_Normal;                        // 法線
    float m_Width;                            // 幅
    float m_Height;                           // 高さ
    std::vector<Triangle> m_Triangles;        // 三角形リスト
    ComPtr<ID3D12Resource> m_VertexBuffer;    // 頂点バッファ
    ComPtr<ID3D12Resource> m_IndexBuffer;     // インデックスバッファ
};