// DXRScene.cpp
#include "DXRScene.h"

void DXRScene::UnInit() {
    Scene::UnInit();  // 基底クラスのUnInit呼び出し
}

void DXRScene::Update() {
    Scene::Update();  // 基底クラスのUpdate呼び出し
}

void DXRScene::Draw() {
    Scene::Draw();    // 基底クラスのDraw呼び出し
}

TLASData DXRScene::GetTLASData() const {
    TLASData tlasData;

    // 全レイヤーからDXRShapeを収集
    auto dxrShapes = GetDXRShapes();

    char debugMsg[256];
    sprintf_s(debugMsg, "GetTLASData: Found %zu DXR shapes\n", dxrShapes.size());
    OutputDebugStringA(debugMsg);

    for ( size_t i = 0; i < dxrShapes.size(); ++i ) {
        auto* shape = dxrShapes[i];
        if ( shape ) {
            BLASData blasData = shape->GetBLASData();

            tlasData.blasDataList.push_back(blasData);
            tlasData.instanceTransforms.push_back(blasData.transform);
        }
    }

    return tlasData;
}

std::vector<DXRShape*> DXRScene::GetDXRShapes() const {
    std::vector<DXRShape*> dxrShapes;

    // 全レイヤーを走査してDXRShapeを抽出
    for ( int i = 0; i < Layer::LayerAll; i++ ) {
        for ( auto* obj : m_GameObject[i] ) {
            DXRShape* dxrShape = dynamic_cast<DXRShape*>( obj );
            if ( dxrShape ) {
                dxrShapes.push_back(dxrShape);
            }
        }
    }

    return dxrShapes;
}