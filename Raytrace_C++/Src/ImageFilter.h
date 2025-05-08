#pragma once
class ImageFilter {
public:
    virtual Vector3 filter(const Vector3& c) const = 0;
};