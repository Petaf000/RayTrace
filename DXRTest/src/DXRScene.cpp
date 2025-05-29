// DXRScene.cpp
#include "DXRScene.h"

void DXRScene::UnInit() {
    Scene::UnInit();  // ���N���X��UnInit�Ăяo��
}

void DXRScene::Update() {
    Scene::Update();  // ���N���X��Update�Ăяo��
}

void DXRScene::Draw() {
    Scene::Draw();    // ���N���X��Draw�Ăяo��
}

TLASData DXRScene::GetTLASData() const {
    TLASData tlasData;

    // �S���C���[����DXRShape�����W
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

    // �S���C���[�𑖍�����DXRShape�𒊏o
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