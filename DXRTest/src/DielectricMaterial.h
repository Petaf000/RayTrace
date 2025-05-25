#pragma once
#include "Material.h"

class DielectricMaterial : public Material {
public:
    // indexOfRefraction�͋��ܗ��i�K���X�͖�1.5�j
    DielectricMaterial(const XMFLOAT3& color, float indexOfRefraction = 1.5f)
        : Material(MaterialType::Dielectric, color, indexOfRefraction) {
    }
};