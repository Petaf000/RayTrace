#include "Lambertian.h"

#include "Ray.h"
#include "HitRec.h"
#include "ScatterRec.h"

bool Lambertian::scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const {
    Vector3 target = hrec.p + hrec.n + random_in_unit_sphere();
    srec.ray = Ray(hrec.p, target - hrec.p);
    srec.albedo = m_albedo;
    return true;
}
