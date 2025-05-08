#pragma once

#include "Material.h"

class Dielectric : public Material {
public:
    Dielectric(float ri)
        : m_ri(ri) {
    }

    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const override;

private:
    float m_ri;
};