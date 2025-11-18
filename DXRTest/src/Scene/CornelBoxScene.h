#pragma once
#include "DXRScene.h"

enum class CornelBoxMaterial : int {
    Red = 0,
    Green,
    White,
    Light,
    Metal,
    Glass
};

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
};