#pragma once

#include "ImageFilter.h"
class GammaFilter : public ImageFilter {
public:
    GammaFilter(float factor) : m_factor(factor) {}
    virtual Vector3 filter(const Vector3& c) const override {
        return linear_to_gamma(c, m_factor);
    }
private:
    float m_factor;
};