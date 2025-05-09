#pragma once

#include "Texture.h"

class ColorTexture : public Texture {
public:
    ColorTexture(const Vector3& c)
        : m_color(c) {
    }

    virtual Vector3 value(float u, float v, const Vector3& p) const override {
        return m_color;
    }
private:
    Vector3 m_color;
};