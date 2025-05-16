#pragma once

class ONB {
public:
    ONB() {}
    inline Vector3& operator[](int i) {
        return m_axis[i];
    }
    inline const Vector3& operator[](int i) const {
        return m_axis[i];
    }
    const Vector3& u() const {
        return m_axis[0];
    }
    const Vector3& v() const {
        return m_axis[1];
    }
    const Vector3& w() const {
        return m_axis[2];
    }
    Vector3 local(float a, float b, float c) const {
        return a * u() + b * v() + c * w();
    }
    Vector3 local(const Vector3& a) const {
        return a.getX() * u() + a.getY() * v() + a.getZ() * w();
    }
    void build_from_w(const Vector3& n) {
        m_axis[2] = normalize(n);
        Vector3 a;
        if ( fabs(w().getX()) > 0.9 ) {
            a = Vector3(0, 1, 0);
        }
        else {
            a = Vector3(1, 0, 0);
        }
        m_axis[1] = normalize(cross(w(), a));
        m_axis[0] = cross(w(), v());
    }

private:
    Vector3 m_axis[3];
};