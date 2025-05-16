#include "CosinePdf.h"

#include "HitRec.h"
#include "ONB.h"

float CosinePdf::value(const HitRec& hrec, const Vector3& direction) const {
    float cosine = dot(normalize(direction), hrec.n);
    if ( cosine > 0 ) {
        return cosine / PI;
    }
    else {
        return 0;
    }
}

Vector3 CosinePdf::generate(const HitRec& hrec) const {
    ONB uvw; uvw.build_from_w(hrec.n);
    Vector3 v = uvw.local(random_cosine_direction());
    return v;
}
