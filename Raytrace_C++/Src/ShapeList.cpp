#include "ShapeList.h"

#include "HitRec.h"

bool ShapeList::hit(const Ray& r, float t0, float t1, HitRec& hrec) const {
    HitRec temp_rec;
    bool hit_anything = false;
    float closest_so_far = t1;
    for ( auto& p : m_list ) {
        if ( p->hit(r, t0, closest_so_far, temp_rec) ) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            hrec = temp_rec;
        }
    }
    return hit_anything;
}
