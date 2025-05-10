#pragma once

#include "Shape.h"

class Translate : public Shape {
public:
    Translate(const ShapePtr& sp, const Vector3& displacement)
        : m_shape(sp)
        , m_offset(displacement) {
    }

    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const override;

private:
    ShapePtr m_shape;
    Vector3 m_offset;
};