#pragma once
#include "Shape.h"

class ShapeList : public Shape {
public:
    ShapeList() {}

    void add(const ShapePtr& shape) {
        m_list.push_back(shape);
    }

    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const override;

private:
    std::vector<ShapePtr> m_list;
};