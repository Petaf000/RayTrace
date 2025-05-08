#pragma once
class Ray {
public:
    Ray() {}
    Ray(const Vector3& o, const Vector3& dir)
        : m_origin(o)
        , m_direction(dir) {
    }

    const Vector3& origin() const { return m_origin; }
    const Vector3& direction() const { return m_direction; }
    Vector3 at(float t) const { return m_origin + t * m_direction; }

private:
    Vector3 m_origin;    // �n�_
    Vector3 m_direction; // �����i�񐳋K���j
};