#pragma once

#include "Material.h"

class Lambertian : public Material {
public:
    Lambertian(const Vector3& c)
        : m_albedo(c) {
    }

    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const override;

private:
    Vector3 m_albedo;
};