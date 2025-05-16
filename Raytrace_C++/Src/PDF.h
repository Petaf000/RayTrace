#pragma once

struct HitRec;

class Pdf {
public:
    virtual float value(const HitRec& hrec, const Vector3& direction) const = 0;
    virtual Vector3 generate(const HitRec& hrec) const = 0;
};