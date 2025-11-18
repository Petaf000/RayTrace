

#include "DXRScene.h"

void DXRScene::UnInit() {
    Scene::UnInit();
}

void DXRScene::Update() {
    Scene::Update();
}

void DXRScene::Draw() {
    Scene::Draw();
}

TLASData DXRScene::GetTLASData() const {
    TLASData tlasData;

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