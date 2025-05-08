#include "Dielectric.h"

#include "Ray.h"
#include "HitRec.h"
#include "ScatterRec.h"

bool Dielectric::scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const {
    Vector3 outward_normal;
    Vector3 reflected = reflect(r.direction(), hrec.n);
    float ni_over_nt;
    float reflect_prob;
    float cosine;
    if ( dot(r.direction(), hrec.n) > 0 ) {
        outward_normal = -hrec.n;
        ni_over_nt = m_ri;
        cosine = m_ri * dot(r.direction(), hrec.n) / length(r.direction());
    }
    else {
        outward_normal = hrec.n;
        ni_over_nt = recip(m_ri);
        cosine = -dot(r.direction(), hrec.n) / length(r.direction());
    }

    srec.albedo = Vector3(1);

    Vector3 refracted;
    if ( refract(-r.direction(), outward_normal, ni_over_nt, refracted) ) {
        reflect_prob = schlick(cosine, m_ri);
    }
    else {
        reflect_prob = 1;
    }

    if ( drand48() < reflect_prob ) {
        srec.ray = Ray(hrec.p, reflected);
    }
    else {
        srec.ray = Ray(hrec.p, refracted);
    }

    return true;
}
