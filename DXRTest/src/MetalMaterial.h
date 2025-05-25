#pragma once
#include "Material.h"

class MetalMaterial : public Material {
public:
    // roughness‚Í0.0`1.0‚Ì”ÍˆÍ(0.0‚ÍŠ®‘S‚È‹¾A1.0‚ÍŠgU“I)
    MetalMaterial(const XMFLOAT3& color, float roughness = 0.0f)
        : Material(MaterialType::Metal, color, roughness) {
    }
};