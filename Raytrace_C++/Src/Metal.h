#pragma once

#include "Material.h"

class Metal : public Material {
public:
    Metal(const TexturePtr& a, float fuzz)
        : m_albedo(a)
        , m_fuzz(fuzz){}

    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const override;

private:
    TexturePtr m_albedo;
	float m_fuzz;
};