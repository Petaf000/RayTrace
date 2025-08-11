// CornelBoxScene.h
#pragma once
#include "DXRScene.h"

class CornelBoxScene : public DXRScene {
public:
    CornelBoxScene() = default;
    virtual ~CornelBoxScene() = default;

    virtual void Init() override;
    virtual void Update() override;;

private:
    void CreateMaterials();
    void CreateWalls();
    void CreateObjects();
    void SetupCamera();

    // マテリアルデータ
    DXRMaterialData m_redMaterial;
    DXRMaterialData m_greenMaterial;
    DXRMaterialData m_whiteMaterial;
    DXRMaterialData m_lightMaterial;
    DXRMaterialData m_metalMaterial;
    DXRMaterialData m_glassMaterial;
};