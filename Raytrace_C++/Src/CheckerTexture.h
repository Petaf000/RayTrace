#pragma once

#include "Texture.h" 

class CheckerTexture : public Texture {
public:
    CheckerTexture(const TexturePtr& t0, const TexturePtr& t1, float freq)
        : m_odd(t0)
        , m_even(t1)
        , m_freq(freq) {
    }

    virtual Vector3 value(float u, float v, const Vector3& p) const override;

private:
    TexturePtr m_odd;
    TexturePtr m_even;
    float m_freq;
};