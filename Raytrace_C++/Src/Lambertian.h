#pragma once

#include "Material.h"

class CosinePdf;

class Lambertian : public Material {
public:
    Lambertian(const TexturePtr& a);

    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const override;
    
    virtual float scattering_pdf(const Ray& r, const HitRec& hrec) const override;

	virtual void set_texture(const TexturePtr& a) override {
		m_albedo = a;
	}
private:
    TexturePtr m_albedo;
    CosinePdf*  m_pdf;
};