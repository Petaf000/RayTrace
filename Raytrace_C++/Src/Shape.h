#pragma once

class Ray;
struct HitRec;
class Shape {
public:
    virtual bool hit(const Ray& r, float t0, float t1, HitRec& hrec) const = 0;
};