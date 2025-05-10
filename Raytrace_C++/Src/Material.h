#pragma once

class Ray;
struct HitRec;
struct ScatterRec;

class Material {
public:
    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const = 0;
    virtual Vector3 emitted(const Ray& r, const HitRec& hrec) const { return Vector3(0); }
};