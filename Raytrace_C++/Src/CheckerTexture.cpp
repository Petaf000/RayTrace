#include "CheckerTexture.h"

Vector3 CheckerTexture::value(float u, float v, const Vector3& p) const {
    float sines = sinf(m_freq * p.getX()) * sinf(m_freq * p.getY()) * sinf(m_freq * p.getZ());
    if ( sines < 0 ) {
        return m_odd->value(u, v, p);
    }
    else {
        return m_even->value(u, v, p);
    }
}
