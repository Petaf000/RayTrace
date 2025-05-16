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

float ShapeList::pdf_value(const Vector3& o, const Vector3& v) const {
    float weight = 1.0f / m_list.size();
    float sum = 0;
    for ( auto& p : m_list ) {
        sum += weight * p->pdf_value(o, v);
    }
    return sum;
}

Vector3 ShapeList::random(const Vector3& o) const {
    size_t n = m_list.size();
    size_t index = size_t(drand48() * n);
    if ( n > 0 && index >= n ) {
        index = n - 1;
    }
    return m_list[index]->random(o);
}
