#pragma once
#include "Shape.h"

// ���`��N���X
class Sphere : public Shape {
public:
    Sphere(const XMFLOAT3& center, float radius, int sliceCount = 32, int stackCount = 16)
        : m_Center(center), m_Radius(radius), m_SliceCount(sliceCount), m_StackCount(stackCount) {
    }

    virtual ~Sphere() override = default;

    // ������
    void Init() override;

    // ���
    void UnInit() override;

    // �W�I���g���L�q���擾
    D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc() override;

private:
    XMFLOAT3 m_Center;                        // ���S
    float m_Radius;                           // ���a
    int m_SliceCount;                         // ���������̕�����
    int m_StackCount;                         // ���������̕�����
    std::vector<XMFLOAT3> m_Vertices;         // ���_�f�[�^
    std::vector<UINT> m_Indices;              // �C���f�b�N�X�f�[�^
    ComPtr<ID3D12Resource> m_VertexBuffer;    // ���_�o�b�t�@
    ComPtr<ID3D12Resource> m_IndexBuffer;     // �C���f�b�N�X�o�b�t�@
};