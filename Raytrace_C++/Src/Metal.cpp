#include "Metal.h"

#include "Ray.h"
#include "HitRec.h"
#include "ScatterRec.h"

#include "Texture.h"

bool Metal::scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const {
    Vector3 reflected = reflect(normalize(r.direction()), hrec.n);
    reflected += m_fuzz * random_in_unit_sphere();
    srec.ray = Ray(hrec.p, reflected);
    srec.albedo = m_albedo->value(hrec.u, hrec.v, hrec.p);
    srec.pdf = nullptr;
	srec.is_specular = true;
    return dot(srec.ray.direction(), hrec.n) > 0;
}
