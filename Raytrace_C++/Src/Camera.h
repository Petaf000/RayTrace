#pragma once
class Camera {
public:
    Camera() {}
    Camera(const Vector3& u, const Vector3& v, const Vector3& w) {
        m_origin = Vector3(0);
        m_uvw[0] = u;
        m_uvw[1] = v;
        m_uvw[2] = w;
    }
    Camera(const Vector3& lookfrom, const Vector3& lookat, const Vector3& vup, float vfov, float aspect) {
        Vector3 u, v, w;
        float halfH = tanf(radians(vfov) / 2.0f);
        float halfW = aspect * halfH;
        m_origin = lookfrom;
        w = normalize(lookfrom - lookat);
        u = normalize(cross(vup, w));
        v = cross(w, u);
        m_uvw[2] = m_origin - halfW * u - halfH * v - w;
        m_uvw[0] = 2.0f * halfW * u;
        m_uvw[1] = 2.0f * halfH * v;
    }

    Ray getRay(float u, float v) const {
        return Ray(m_origin, m_uvw[2] + m_uvw[0] * u + m_uvw[1] * v - m_origin);
    }

private:
    Vector3 m_origin;  // 位置
    Vector3 m_uvw[3];  // 直交基底ベクトル
};