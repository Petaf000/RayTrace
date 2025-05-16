#pragma once

class Ray;
struct HitRec;
struct ScatterRec;

class Material {
public:
    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const = 0;
    virtual Vector3 emitted(const Ray& r, const HitRec& hrec) const { return Vector3(0); }
    virtual float scattering_pdf(const Ray& r, const HitRec& hrec) const { return 0; }
	virtual void set_texture(const TexturePtr& tex) { }
};