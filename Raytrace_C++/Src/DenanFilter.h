#pragma once

#include "ImageFilter.h"

class DenanFilter : public ImageFilter {
public:
    DenanFilter() {}
    virtual Vector3 filter(const Vector3& c) const override {
        return Vector3(
            !( c[0] == c[0] ) ? 1.0f : c[0],
            !( c[1] == c[1] ) ? 0 : c[1],
            !( c[2] == c[2] ) ? 1.0f : c[2]);
    }
};