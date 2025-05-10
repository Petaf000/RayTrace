#include "Translate.h"

#include "Ray.h"
#include "HitRec.h"

bool Translate::hit(const Ray& r, float t0, float t1, HitRec& hrec) const {
    Ray move_r(r.origin() - m_offset, r.direction());
    if ( m_shape->hit(move_r, t0, t1, hrec) ) {
        hrec.p += m_offset;
        return true;
    }
    else {
        return false;
    }
}
