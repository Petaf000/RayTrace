#pragma once
#include "Material.h"

class EmissiveMaterial : public Material {
public:
    // intensity‚Í”­Œõ‹­“x
    EmissiveMaterial(const XMFLOAT3& color, float intensity = 1.0f)
        : Material(MaterialType::Emissive, color, intensity) {
    }
};