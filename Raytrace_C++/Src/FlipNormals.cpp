#include "FlipNormals.h"

#include "Ray.h"
#include "HitRec.h"

bool FlipNormals::hit(const Ray& r, float t0, float t1, HitRec& hrec) const {
    if ( m_shape->hit(r, t0, t1, hrec) ) {
        hrec.n = -hrec.n;
        return true;
    }
    else {
        return false;
    }
}
