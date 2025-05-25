#pragma once

// 前方宣言
class Material;

// 基本形状クラス
class Shape {
public:
    virtual ~Shape() = default;

    // 形状の初期化
    virtual void Init() = 0;

    // 形状のリソース解放
    virtual void UnInit() = 0;

    // レイトレーシング用のジオメトリ記述を取得
    virtual D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc() = 0;

    // マテリアルの設定
    void SetMaterial(Material* material) { m_Material = material; }

    // マテリアルの取得
    Material* GetMaterial() const { return m_Material; }

protected:
    Material* m_Material = nullptr;
};
