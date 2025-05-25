#pragma once
#include "Scene.h"

#include "Shape.h"
#include "Material.h"

// コーネルボックスシーンクラス
class CornellBoxScene : public Scene {
public:
    CornellBoxScene() = default;
    virtual ~CornellBoxScene() override = default;

    // 初期化
    void Init() override;

    // 解放
    void UnInit() override;

    // 更新
    void Update() override;

    // 描画
    void Draw() override;

private:
    // 各オブジェクトとマテリアルへのポインタ
    std::vector<std::unique_ptr<Shape>> m_Shapes;
    std::vector<std::unique_ptr<Material>> m_Materials;
};