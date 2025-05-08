#pragma once

class Ray;
struct HitRec;
struct ScatterRec;

class Material {
public:
    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const = 0;
};