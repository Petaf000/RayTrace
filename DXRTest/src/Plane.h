#pragma once
#include "Shape.h"
#include "Triangle.h"

// ���ʌ`��N���X
class Plane : public Shape {
public:
    Plane(const XMFLOAT3& origin, const XMFLOAT3& normal, float width, float height)
        : m_Origin(origin), m_Normal(normal), m_Width(width), m_Height(height) {
    }

    virtual ~Plane() override = default;

    // ������
    void Init() override;

    // ���
    void UnInit() override;

    // �W�I���g���L�q���擾
    D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc() override;

private:
    XMFLOAT3 m_Origin;                        // ���_
    XMFLOAT3 m_Normal;                        // �@��
    float m_Width;                            // ��
    float m_Height;                           // ����
    std::vector<Triangle> m_Triangles;        // �O�p�`���X�g
    ComPtr<ID3D12Resource> m_VertexBuffer;    // ���_�o�b�t�@
    ComPtr<ID3D12Resource> m_IndexBuffer;     // �C���f�b�N�X�o�b�t�@
};