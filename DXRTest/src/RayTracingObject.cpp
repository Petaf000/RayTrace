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
    // ���ۂ̕`��̓��C�g���[�V���O�p�C�v���C���ōs�����߁A�����ł͉������Ȃ�
}

void RayTracingObject::Update() {
    // �K�v�ɉ����ăI�u�W�F�N�g�̍X�V���s��
}