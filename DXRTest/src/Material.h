#pragma once
#include "MaterialType.h"
#include <DirectXMath.h>

using namespace DirectX;

// �}�e���A���N���X
class Material {
public:
    Material(MaterialType type, const XMFLOAT3& color, float param = 0.0f)
        : m_Type(type), m_Color(color), m_Param(param) {
    }

    virtual ~Material() = default;

    // �}�e���A���^�C�v�̎擾
    MaterialType GetType() const { return m_Type; }

    // �F�̎擾
    XMFLOAT3 GetColor() const { return m_Color; }

    // �p�����[�^�̎擾�i�����̏ꍇ�͑e���A�U�d�̂̏ꍇ�͋��ܗ��A�����̂̏ꍇ�͋��x�j
    float GetParam() const { return m_Param; }

    // �F�̐ݒ�
    void SetColor(const XMFLOAT3& color) { m_Color = color; }

    // �p�����[�^�̐ݒ�
    void SetParam(float param) { m_Param = param; }

private:
    MaterialType m_Type;   // �}�e���A���^�C�v
    XMFLOAT3 m_Color;      // �F
    float m_Param;         // �ǉ��p�����[�^
};