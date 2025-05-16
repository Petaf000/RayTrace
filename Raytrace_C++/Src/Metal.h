#pragma once

#include "Material.h"

class Metal : public Material {
public:
    Metal(const TexturePtr& a, float fuzz)
        : m_albedo(a)
        , m_fuzz(fuzz){}

    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const override;

	virtual void set_texture(const TexturePtr& a) override {
		m_albedo = a;
	}

	void set_fuzz(float fuzz) {
		m_fuzz = fuzz;
	}

private:
    TexturePtr m_albedo;
	float m_fuzz;
};