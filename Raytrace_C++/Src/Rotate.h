#pragma once

#include "Shape.h"

class Rotate : public Shape {
public:
    Rotate(const ShapePtr& sp, const Vector3& axis, float angle)
        : m_shape(sp)
        , m_quat(Quat::rotation(radians(angle), axis)) {
    }

    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const override;

private:
    ShapePtr m_shape;
    Quat m_quat;
};