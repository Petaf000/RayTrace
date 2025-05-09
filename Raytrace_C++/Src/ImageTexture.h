#pragma once

#include "Texture.h"

class ImageTexture : public Texture {
public:
    ImageTexture(const char* name) {
        int nn;
        m_texels = stbi_load(name, &m_width, &m_height, &nn, 0);
    }

    virtual ~ImageTexture() {
        stbi_image_free(m_texels);
    }

    virtual Vector3 value(float u, float v, const Vector3& p) const override {
        int i = (u)*m_width;
        int j = ( 1 - v ) * m_height - 0.001;
        return sample(i, j);
    }

    Vector3 sample(int u, int v) const {
        u = u < 0 ? 0 : u >= m_width ? m_width - 1 : u;
        v = v < 0 ? 0 : v >= m_height ? m_height - 1 : v;
        return Vector3(
            int(m_texels[3 * u + 3 * m_width * v]) / 255.0,
            int(m_texels[3 * u + 3 * m_width * v + 1]) / 255.0,
            int(m_texels[3 * u + 3 * m_width * v + 2]) / 255.0);
    }

private:
    int m_width;
    int m_height;
    unsigned char* m_texels;
};