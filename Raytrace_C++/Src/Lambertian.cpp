#include "Lambertian.h"

#include "Ray.h"
#include "HitRec.h"
#include "ScatterRec.h"
#include "ONB.h"
#include "CosinePdf.h"

#include "Texture.h"

Lambertian::Lambertian(const TexturePtr& a)
    : m_albedo(a) {
    m_pdf = new CosinePdf();
}

bool Lambertian::scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const {
    srec.albedo = m_albedo->value(hrec.u, hrec.v, hrec.p);
    srec.pdf = m_pdf;
    srec.is_specular = false;
    return true;
}

float Lambertian::scattering_pdf(const Ray& r, const HitRec& hrec) const {
    return std::max(dot(hrec.n, normalize(r.direction())), 0.0f) / PI;
}
