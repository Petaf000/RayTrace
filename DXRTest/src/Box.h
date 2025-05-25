#pragma once
#include "Shape.h"
#include "Triangle.h"

// �{�b�N�X�`��N���X
class Box : public Shape {
public:
    Box(const XMFLOAT3& min, const XMFLOAT3& max) : m_Min(min), m_Max(max) {}
    virtual ~Box() override = default;

    // ������
    void Init() override;

    // ���
    void UnInit() override;

    // �W�I���g���L�q���擾
    D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc() override;

private:
    XMFLOAT3 m_Min;                           // �ŏ��_
    XMFLOAT3 m_Max;                           // �ő�_
    std::vector<Triangle> m_Triangles;        // �O�p�`���X�g
    ComPtr<ID3D12Resource> m_VertexBuffer;    // ���_�o�b�t�@
    ComPtr<ID3D12Resource> m_IndexBuffer;     // �C���f�b�N�X�o�b�t�@
};