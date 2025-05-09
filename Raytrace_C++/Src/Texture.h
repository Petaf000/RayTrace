#pragma once

class Texture {
public:
    virtual Vector3 value(float u, float v, const Vector3& p) const = 0;
};