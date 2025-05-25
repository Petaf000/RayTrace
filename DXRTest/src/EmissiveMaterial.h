#pragma once
#include "Material.h"

class EmissiveMaterial : public Material {
public:
    // intensity�͔������x
    EmissiveMaterial(const XMFLOAT3& color, float intensity = 1.0f)
        : Material(MaterialType::Emissive, color, intensity) {
    }
};