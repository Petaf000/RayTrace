#include "Sphere.h"

#include "Ray.h"
#include "HitRec.h"
#include "ONB.h"

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

float Sphere::pdf_value(const Vector3& o, const Vector3& v) const {
    HitRec hrec;
    if ( this->hit(Ray(o, v), 0.001f, FLT_MAX, hrec) ) {
        float dd = lengthSqr(m_center - o);
        float rr = std::min(pow2(m_radius), dd);
        float cos_theta_max = sqrtf(1.0f - rr * recip(dd));
        float solid_angle = PI2 * ( 1.0f - cos_theta_max );
        return recip(solid_angle);
    }
    else {
        return 0;
    }
}

Vector3 Sphere::random(const Vector3& o) const {
    Vector3 direction = m_center - o;
    float distance_squared = lengthSqr(direction);
    ONB uvw; uvw.build_from_w(direction);
    Vector3 v = uvw.local(random_to_sphere(m_radius, distance_squared));
    return v;
}
