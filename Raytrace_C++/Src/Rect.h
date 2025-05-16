#pragma once

#include "Shape.h"  

class Rect : public Shape {
public:
    enum AxisType {
        kXY = 0,
        kXZ,
        kYZ
    };
    Rect() {}
    Rect(float x0, float x1, float y0, float y1, float k, AxisType axis, const MaterialPtr& m)
        : m_x0(x0)
        , m_x1(x1)
        , m_y0(y0)
        , m_y1(y1)
        , m_k(k)
        , m_axis(axis)
        , m_material(m) {
    }

    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const override;

    virtual float pdf_value(const Vector3& o, const Vector3& v) const override;

    virtual Vector3 random(const Vector3& o) const override;

private:
    float m_x0, m_x1, m_y0, m_y1, m_k;
    AxisType m_axis;
    MaterialPtr m_material;
};