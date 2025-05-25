#pragma once
#include "Scene.h"

#include "Shape.h"
#include "Material.h"

// �R�[�l���{�b�N�X�V�[���N���X
class CornellBoxScene : public Scene {
public:
    CornellBoxScene() = default;
    virtual ~CornellBoxScene() override = default;

    // ������
    void Init() override;

    // ���
    void UnInit() override;

    // �X�V
    void Update() override;

    // �`��
    void Draw() override;

private:
    // �e�I�u�W�F�N�g�ƃ}�e���A���ւ̃|�C���^
    std::vector<std::unique_ptr<Shape>> m_Shapes;
    std::vector<std::unique_ptr<Material>> m_Materials;
};