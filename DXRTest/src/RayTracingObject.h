#pragma once
#include "GameObject.h"
#include "Shape.h"

// レイトレーシングオブジェクトクラス
class RayTracingObject : public GameObject {
public:
    RayTracingObject() : GameObject() {}
    RayTracingObject(Shape* shape) : GameObject(), m_Shape(shape) {}
    RayTracingObject(OPosition pos) : GameObject(pos) {}
    RayTracingObject(OPosition pos, Shape* shape) : GameObject(pos), m_Shape(shape) {}
    RayTracingObject(Transform transform, Shape* shape) : GameObject(transform), m_Shape(shape) {}
    virtual ~RayTracingObject() override = default;

    // 初期化
    void Init() override;

    // 解放
    void UnInit() override;

    // 描画
    void Draw() override;

    // 更新
    void Update() override;

    // 形状の設定
    void SetShape(Shape* shape) { m_Shape = shape; }

    // 形状の取得
    Shape* GetShape() const { return m_Shape; }

private:
    Shape* m_Shape = nullptr;  // 形状
};