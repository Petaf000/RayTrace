#pragma once
#include "Image.h"
#include "Ray.h"
#include "Camera.h"
#include "Shape.h"

class Scene {
public:
    Scene(const char* fileName, int width, int height, int sample)
        : m_image(std::make_unique<Image>(width, height))
        , m_backColor(0.2f)
        , m_samples(sample)
        , m_filename(fileName) {}

    void build();

    float hit_sphere(const Vector3& center, float radius, const Ray& r) const;
    Vector3 color(const Ray& r, const Shape* world, const Shape* light, int depth) const;

    Vector3 background(const Vector3& d) const {
        return m_backColor;
    }

    Vector3 backgroundSky(const Vector3& d) const {
        Vector3 v = normalize(d);
        float t = 0.5f * ( v.getY() + 1.0f );
        return lerp(t, Vector3(1), Vector3(0.5f, 0.7f, 1.0f));
    }

    void render();

	const char* getFilename() const {
		return m_filename.c_str();
	}

private:
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Image> m_image;
    Vector3 m_backColor;
	std::string m_filename;
    std::unique_ptr<Shape> m_world;
    int m_samples;
    std::unique_ptr<Shape> m_light;
};
