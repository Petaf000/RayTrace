#pragma once

#include "Material.h"

class Lambertian : public Material {
public:
    Lambertian(const TexturePtr& a)
        : m_albedo(a) {
    }

    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const override;

private:
    TexturePtr m_albedo;
};