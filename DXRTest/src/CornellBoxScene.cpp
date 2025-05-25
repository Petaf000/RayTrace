#include "CornellBoxScene.h"

#include "Box.h"
#include "Plane.h"
#include "Sphere.h"
#include "RayTracingObject.h"
#include "LambertMaterial.h"
#include "MetalMaterial.h"
#include "DielectricMaterial.h"
#include "EmissiveMaterial.h"

void CornellBoxScene::Init() {
    // 壁、床、天井、光源のマテリアルを作成
    m_Materials.push_back(std::make_unique<LambertMaterial>(XMFLOAT3(0.9f, 0.9f, 0.9f))); // 白（床、天井、奥の壁）
    m_Materials.push_back(std::make_unique<LambertMaterial>(XMFLOAT3(0.9f, 0.1f, 0.1f))); // 赤（右壁）
    m_Materials.push_back(std::make_unique<LambertMaterial>(XMFLOAT3(0.1f, 0.9f, 0.1f))); // 緑（左壁）
    m_Materials.push_back(std::make_unique<EmissiveMaterial>(XMFLOAT3(1.0f, 1.0f, 1.0f), 5.0f)); // 光源

    // オブジェクトのマテリアルを作成
    m_Materials.push_back(std::make_unique<LambertMaterial>(XMFLOAT3(0.7f, 0.7f, 0.7f))); // グレー（ボックス）
    m_Materials.push_back(std::make_unique<MetalMaterial>(XMFLOAT3(0.8f, 0.8f, 0.8f), 0.1f)); // メタル（球）
    m_Materials.push_back(std::make_unique<DielectricMaterial>(XMFLOAT3(1.0f, 1.0f, 1.0f), 1.5f)); // ガラス（球）

    // コーネルボックスの各面を作成
    // 床
    auto floor = std::make_unique<Plane>(
        XMFLOAT3(0.0f, 0.0f, 0.0f),   // 原点
        XMFLOAT3(0.0f, 1.0f, 0.0f),   // 上向き法線
        10.0f,                         // 幅
        10.0f                          // 奥行き
    );
    floor->SetMaterial(m_Materials[0].get());
    m_Shapes.push_back(std::move(floor));

    // 天井
    auto ceiling = std::make_unique<Plane>(
        XMFLOAT3(0.0f, 10.0f, 0.0f),  // 天井の高さ
        XMFLOAT3(0.0f, -1.0f, 0.0f),  // 下向き法線
        10.0f,                         // 幅
        10.0f                          // 奥行き
    );
    ceiling->SetMaterial(m_Materials[0].get());
    m_Shapes.push_back(std::move(ceiling));

    // 奥の壁
    auto backWall = std::make_unique<Plane>(
        XMFLOAT3(0.0f, 5.0f, 10.0f),  // 奥の壁の中心
        XMFLOAT3(0.0f, 0.0f, -1.0f),  // 前向き法線
        10.0f,                         // 幅
        10.0f                          // 高さ
    );
    backWall->SetMaterial(m_Materials[0].get());
    m_Shapes.push_back(std::move(backWall));

    // 右の壁（赤）
    auto rightWall = std::make_unique<Plane>(
        XMFLOAT3(5.0f, 5.0f, 5.0f),   // 右の壁の中心
        XMFLOAT3(-1.0f, 0.0f, 0.0f),  // 左向き法線
        10.0f,                         // 奥行き
        10.0f                          // 高さ
    );
    rightWall->SetMaterial(m_Materials[1].get());
    m_Shapes.push_back(std::move(rightWall));

    // 左の壁（緑）
    auto leftWall = std::make_unique<Plane>(
        XMFLOAT3(-5.0f, 5.0f, 5.0f),  // 左の壁の中心
        XMFLOAT3(1.0f, 0.0f, 0.0f),   // 右向き法線
        10.0f,                         // 奥行き
        10.0f                          // 高さ
    );
    leftWall->SetMaterial(m_Materials[2].get());
    m_Shapes.push_back(std::move(leftWall));

    // 光源（天井の小さな四角形）
    auto light = std::make_unique<Plane>(
        XMFLOAT3(0.0f, 9.99f, 5.0f),  // 天井すれすれ
        XMFLOAT3(0.0f, -1.0f, 0.0f),  // 下向き法線
        2.0f,                          // 幅
        2.0f                           // 奥行き
    );
    light->SetMaterial(m_Materials[3].get());
    m_Shapes.push_back(std::move(light));

    // ボックス
    auto box = std::make_unique<Box>(
        XMFLOAT3(-1.5f, 0.0f, 4.0f),  // 最小点
        XMFLOAT3(1.5f, 3.0f, 6.0f)    // 最大点
    );
    box->SetMaterial(m_Materials[4].get());
    m_Shapes.push_back(std::move(box));

    // メタル球
    auto metalSphere = std::make_unique<Sphere>(
        XMFLOAT3(2.0f, 1.5f, 3.0f),   // 中心
        1.5f                           // 半径
    );
    metalSphere->SetMaterial(m_Materials[5].get());
    m_Shapes.push_back(std::move(metalSphere));

    // ガラス球
    auto glassSphere = std::make_unique<Sphere>(
        XMFLOAT3(-2.0f, 1.5f, 2.0f),  // 中心
        1.5f                           // 半径
    );
    glassSphere->SetMaterial(m_Materials[6].get());
    m_Shapes.push_back(std::move(glassSphere));

    // シーンにオブジェクトを追加
    for ( size_t i = 0; i < m_Shapes.size(); i++ ) {
        AddGameObject<RayTracingObject>(Layer::Gameobject3D, m_Shapes[i].get());
    }
}

void CornellBoxScene::UnInit() {
    // 親クラスの解放処理
    Scene::UnInit();

    // 各リソースの解放
    m_Shapes.clear();
    m_Materials.clear();
}

void CornellBoxScene::Update() {
    // 親クラスの更新処理
    Scene::Update();
}

void CornellBoxScene::Draw() {
    // 親クラスの描画処理
    Scene::Draw();
}