#pragma once

#include "Shape.h"

class FlipNormals : public Shape {
public:
    FlipNormals(const ShapePtr& shape)
        : m_shape(shape) {
    }

    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const override;

private:
    ShapePtr m_shape;
};