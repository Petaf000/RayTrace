#include "Rect.h"

#include "Ray.h"
#include "HitRec.h"

bool Rect::hit(const Ray& r, float t0, float t1, HitRec& hrec) const {

    int xi, yi, zi;
    Vector3 axis;
    switch ( m_axis ) {
        case kXY: xi = 0; yi = 1; zi = 2; axis = Vector3::zAxis(); break;
        case kXZ: xi = 0; yi = 2; zi = 1; axis = Vector3::yAxis(); break;
        case kYZ: xi = 1; yi = 2; zi = 0; axis = Vector3::xAxis(); break;
    }

    float t = ( m_k - r.origin()[zi] ) / r.direction()[zi];
    if ( t < t0 || t > t1 ) {
        return false;
    }

    float x = r.origin()[xi] + t * r.direction()[xi];
    float y = r.origin()[yi] + t * r.direction()[yi];
    if ( x < m_x0 || x > m_x1 || y < m_y0 || y > m_y1 ) {
        return false;
    }

    hrec.u = ( x - m_x0 ) / ( m_x1 - m_x0 );
    hrec.v = ( y - m_y0 ) / ( m_y1 - m_y0 );
    hrec.t = t;
    hrec.mat = m_material;
    hrec.p = r.at(t);
    hrec.n = axis;
    return true;
}
