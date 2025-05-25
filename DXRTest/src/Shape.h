#pragma once

// �O���錾
class Material;

// ��{�`��N���X
class Shape {
public:
    virtual ~Shape() = default;

    // �`��̏�����
    virtual void Init() = 0;

    // �`��̃��\�[�X���
    virtual void UnInit() = 0;

    // ���C�g���[�V���O�p�̃W�I���g���L�q���擾
    virtual D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc() = 0;

    // �}�e���A���̐ݒ�
    void SetMaterial(Material* material) { m_Material = material; }

    // �}�e���A���̎擾
    Material* GetMaterial() const { return m_Material; }

protected:
    Material* m_Material = nullptr;
};
