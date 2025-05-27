#pragma once

#include "Scene.h"
#include "DXRRenderer.h"
#include "DXRGameObjects.h"

// CPUレイトレのSceneに対応するDXRシーン
class DXRScene : public Scene {
public:
    DXRScene();
    virtual ~DXRScene() = default;

    virtual void Init() override;
    virtual void UnInit() override;
    virtual void Update() override;
    virtual void Draw() override;

    // DXR用メソッド
    void InitDXR(DXRRenderer* renderer);
    void BuildAccelerationStructures();
    void UpdateCamera();

    // シーンデータ取得
    const std::vector<DXRMaterial>& GetMaterials() const { return m_materials; }
    const std::vector<DXRInstance>& GetInstances() const { return m_instances; }
    const DXRCameraData& GetCamera() const { return m_camera; }
    const DXRGlobalConstants& GetGlobalConstants() const { return m_globalConstants; }

    // DXRオブジェクト専用追加メソッド
    template<typename T = DXRShape, typename... Args>
    T* AddDXRGameObject(Layer layer, Args&&... args) {
        T* obj = new T(std::forward<Args>(args)...);
        obj->Init();
        m_GameObject[layer].push_back(obj);

        // DXRShapeの場合は管理リストに追加
        DXRShape* shape = dynamic_cast<DXRShape*>( obj );
        if ( shape ) {
            m_dxrShapes.push_back(shape);
        }

        return obj;
    }

protected:
    // コーネルボックスシーン構築
    virtual void BuildCornellBoxScene();

    // マテリアル作成
    void CreateMaterials();

    // インスタンス更新
    void UpdateInstances();

    // カメラ更新
    void UpdateCameraData();

protected:
    DXRRenderer* m_dxrRenderer;

    // DXRシーンデータ
    std::vector<DXRMaterial> m_materials;
    std::vector<DXRInstance> m_instances;
    DXRCameraData m_camera;
    DXRGlobalConstants m_globalConstants;

    // DXRオブジェクト管理
    std::vector<DXRShape*> m_dxrShapes;

    // カメラパラメータ
    XMFLOAT3 m_cameraPosition;
    XMFLOAT3 m_cameraTarget;
    XMFLOAT3 m_cameraUp;
    float m_fov;
    float m_aspect;

    // レンダリングパラメータ
    int m_maxDepth;
    int m_sampleCount;
    XMFLOAT4 m_backgroundColor;
    int m_frameIndex;
};

// 具体的なコーネルボックスシーン実装
class CornellBoxDXRScene : public DXRScene {
public:
    CornellBoxDXRScene();
    virtual ~CornellBoxDXRScene() = default;

    virtual void Init() override;

private:
    virtual void BuildCornellBoxScene() override;
};