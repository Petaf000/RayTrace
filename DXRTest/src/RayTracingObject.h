#pragma once
#include "GameObject.h"
#include "Shape.h"

// ���C�g���[�V���O�I�u�W�F�N�g�N���X
class RayTracingObject : public GameObject {
public:
    RayTracingObject() : GameObject() {}
    RayTracingObject(Shape* shape) : GameObject(), m_Shape(shape) {}
    RayTracingObject(OPosition pos) : GameObject(pos) {}
    RayTracingObject(OPosition pos, Shape* shape) : GameObject(pos), m_Shape(shape) {}
    RayTracingObject(Transform transform, Shape* shape) : GameObject(transform), m_Shape(shape) {}
    virtual ~RayTracingObject() override = default;

    // ������
    void Init() override;

    // ���
    void UnInit() override;

    // �`��
    void Draw() override;

    // �X�V
    void Update() override;

    // �`��̐ݒ�
    void SetShape(Shape* shape) { m_Shape = shape; }

    // �`��̎擾
    Shape* GetShape() const { return m_Shape; }

private:
    Shape* m_Shape = nullptr;  // �`��
};