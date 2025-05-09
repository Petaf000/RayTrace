#pragma once

#include "Material.h"

class Metal : public Material {
public:
    Metal(const Vector3& c, float fuzz)
        : m_albedo(c)
        , m_fuzz(fuzz){}

    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const override;

private:
    Vector3 m_albedo;
	float m_fuzz;
};