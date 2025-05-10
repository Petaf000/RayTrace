#pragma once
#include "Shape.h"

class ShapeList;

class Box : public Shape {
public:
    Box() {}
    Box(const Vector3& p0, const Vector3& p1, const MaterialPtr& m);

    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const override;

private:
    Vector3 m_p0, m_p1;
    std::unique_ptr<ShapeList> m_list;
};