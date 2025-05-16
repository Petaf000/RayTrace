#pragma once

#include "PDF.h"

class CosinePdf : public Pdf {
public:
    CosinePdf() {}

    virtual float value(const HitRec& hrec, const Vector3& direction) const override;

    virtual Vector3 generate(const HitRec& hrec) const override;
};