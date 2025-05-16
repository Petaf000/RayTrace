#pragma once

class Ray;
struct HitRec;
class Shape {
public:
    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const = 0;
    virtual float pdf_value(const Vector3& o, const Vector3& v) const { return 0; }
    virtual Vector3 random(const Vector3& o) const { return Vector3(1, 0, 0); }
};