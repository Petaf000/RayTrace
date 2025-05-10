#include "Rotate.h"

#include "Ray.h"
#include "HitRec.h"

bool Rotate::hit(const Ray& r, float t0, float t1, HitRec& hrec) const {
    Quat revq = conj(m_quat);
    Vector3 origin = rotate(revq, r.origin());
    Vector3 direction = rotate(revq, r.direction());
    Ray rot_r(origin, direction);
    if ( m_shape->hit(rot_r, t0, t1, hrec) ) {
        hrec.p = rotate(m_quat, hrec.p);
        hrec.n = rotate(m_quat, hrec.n);
        return true;
    }
    else {
        return false;
    }
}
