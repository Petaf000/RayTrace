#include "Sphere.h"

#include "Ray.h"
#include "HitRec.h"

bool Sphere::hit(const Ray& r, float t0, float t1, HitRec& hrec) const {
    Vector3 oc = r.origin() - m_center;
    float a = dot(r.direction(), r.direction());
    float b = 2.0f * dot(oc, r.direction());
    float c = dot(oc, oc) - pow2(m_radius);
    float D = b * b - 4 * a * c;
    if ( D > 0 ) {
        float root = sqrtf(D);
        float temp = ( -b - root ) / ( 2.0f * a );
        if ( temp < t1 && temp > t0 ) {
            hrec.t = temp;
            hrec.p = r.at(hrec.t);
            hrec.n = ( hrec.p - m_center ) / m_radius;
            hrec.mat = m_material;
            get_sphere_uv(hrec.n, hrec.u, hrec.v);
            return true;
        }
        temp = ( -b + root ) / ( 2.0f * a );
        if ( temp < t1 && temp > t0 ) {
            hrec.t = temp;
            hrec.p = r.at(hrec.t);
            hrec.n = ( hrec.p - m_center ) / m_radius;
            hrec.mat = m_material;
            get_sphere_uv(hrec.n, hrec.u, hrec.v);
            return true;
        }
    }

    return false;
}
