#pragma once
#include "MaterialType.h"
#include <DirectXMath.h>

using namespace DirectX;

// マテリアルクラス
class Material {
public:
    Material(MaterialType type, const XMFLOAT3& color, float param = 0.0f)
        : m_Type(type), m_Color(color), m_Param(param) {
    }

    virtual ~Material() = default;

    // マテリアルタイプの取得
    MaterialType GetType() const { return m_Type; }

    // 色の取得
    XMFLOAT3 GetColor() const { return m_Color; }

    // パラメータの取得（金属の場合は粗さ、誘電体の場合は屈折率、発光体の場合は強度）
    float GetParam() const { return m_Param; }

    // 色の設定
    void SetColor(const XMFLOAT3& color) { m_Color = color; }

    // パラメータの設定
    void SetParam(float param) { m_Param = param; }

private:
    MaterialType m_Type;   // マテリアルタイプ
    XMFLOAT3 m_Color;      // 色
    float m_Param;         // 追加パラメータ
};