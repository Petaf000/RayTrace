#pragma once
#include "Material.h"

class LambertMaterial : public Material {
public:
    LambertMaterial(const XMFLOAT3& color)
        : Material(MaterialType::Lambert, color) {
    }
};