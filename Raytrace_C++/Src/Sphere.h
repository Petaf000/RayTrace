#pragma once
#include "Shape.h"

class Sphere : public Shape {
public:
    Sphere() {}
    Sphere(const Vector3& c, float r, const MaterialPtr& mat)
        : m_center(c)
        , m_radius(r)
        , m_material(mat) {
    }

    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const override;

private:
    Vector3 m_center;
    float m_radius;
    MaterialPtr m_material;
};