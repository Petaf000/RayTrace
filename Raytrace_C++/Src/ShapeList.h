#pragma once
#include "Shape.h"

class ShapeList : public Shape {
public:
    ShapeList() {}

    void add(const ShapePtr& shape) {
        m_list.push_back(shape);
    }

    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const override;

    virtual float pdf_value(const Vector3& o, const Vector3& v) const override;

    virtual Vector3 random(const Vector3& o) const override;

private:
    std::vector<ShapePtr> m_list;
};