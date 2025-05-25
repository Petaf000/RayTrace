#include "RayTracingObject.h"

void RayTracingObject::Init() {
    if ( m_Shape ) {
        m_Shape->Init();
    }
}

void RayTracingObject::UnInit() {
    if ( m_Shape ) {
        m_Shape->UnInit();
    }
}

void RayTracingObject::Draw() {
    // 実際の描画はレイトレーシングパイプラインで行うため、ここでは何もしない
}

void RayTracingObject::Update() {
    // 必要に応じてオブジェクトの更新を行う
}