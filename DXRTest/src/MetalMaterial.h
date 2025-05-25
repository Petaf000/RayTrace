#pragma once
#include "Material.h"

class MetalMaterial : public Material {
public:
    // roughness��0.0�`1.0�͈̔�(0.0�͊��S�ȋ��A1.0�͊g�U�I)
    MetalMaterial(const XMFLOAT3& color, float roughness = 0.0f)
        : Material(MaterialType::Metal, color, roughness) {
    }
};