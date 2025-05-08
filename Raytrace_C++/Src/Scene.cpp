#include "Scene.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#define NUM_THREAD 4
#define MAX_DEPTH 50

// Objects
#include "ShapeList.h"
#include "Sphere.h"

// Structs
#include "HitRec.h"
#include "ScatterRec.h"

// Materials
#include "Lambertian.h"
#include "Metal.h"
#include "Dielectric.h"

void Scene::build() {
    // Camera

    Vector3 lookfrom(13, 2, 3);
    Vector3 lookat(0, 0, 0);
    Vector3 vup(0, 1, 0);
    float aspect = float(m_image->width()) / float(m_image->height());
    m_camera = std::make_unique<Camera>(lookfrom, lookat, vup, 20, aspect);

    // AddObjects

    ShapeList* world = new ShapeList();

    int N = 11;
    for ( int i = -N; i < N; ++i ) {
        for ( int j = -N; j < N; ++j ) {
            float choose_mat = drand48();
            Vector3 center(i + 0.9f * drand48(), 0.2f, j + 0.9f * drand48());
            if ( length(center - Vector3(4, 0.2, 0)) > 0.9f ) {
                if ( choose_mat < 0.8f ) {
                    world->add(std::make_shared<Sphere>(
                        center, 0.2f,
                        std::make_shared<Lambertian>(mulPerElem(random_vector(), random_vector()))));
                }
                else if ( choose_mat < 0.95f ) {
                    world->add(std::make_shared<Sphere>(
                        center, 0.2f,
                        std::make_shared<Metal>(0.5f * ( random_vector() + Vector3(1) ), 0.5f * drand48())));
                }
                else {
                    world->add(std::make_shared<Sphere>(
                        center, 0.2f,
                        std::make_shared<Dielectric>(1.5f)));
                }
            }
        }
    }

    world->add(std::make_shared<Sphere>(
        Vector3(0, -1000, -1), 1000,
        std::make_shared<Lambertian>(Vector3(0.5f, 0.5f, 0.5f))));
    m_world.reset(world);
}

float Scene::hit_sphere(const Vector3& center, float radius, const Ray& r) const {
    Vector3 oc = r.origin() - center;
    float a = dot(r.direction(), r.direction());
    float b = 2.0f * dot(r.direction(), oc);
    float c = dot(oc, oc) - pow2(radius);
    float D = b * b - 4 * a * c;
    if ( D < 0 ) {
        return -1.0f;
    }
    else {
        return ( -b - sqrtf(D) ) / ( 2.0f * a );
    }
}

Vector3 Scene::color(const Ray& r, const Shape* world, int depth) const {
    HitRec hrec;
    if ( world->hit(r, 0.001f, FLT_MAX, hrec) ) {
        ScatterRec srec;
        if ( depth < MAX_DEPTH && hrec.mat->scatter(r, hrec, srec) ) {
            return mulPerElem(srec.albedo, color(srec.ray, world, depth + 1));
        }
        else {
            return Vector3(0);
        }
    }
    return backgroundSky(r.direction());
}

void Scene::render() {
    build();

    int nx = m_image->width();
    int ny = m_image->height();
#pragma omp parallel for schedule(dynamic, 1) num_threads(NUM_THREAD)
    for ( int j = 0; j < ny; ++j ) {
        std::cerr << "Rendering (y = " << j << ") " << ( 100.0 * j / ( ny - 1 ) ) << "%" << std::endl;
        for ( int i = 0; i < nx; ++i ) {
            Vector3 c(0);
            for ( int s = 0; s < m_samples; ++s ) {
                float u = ( float(i) + drand48() ) / float(nx);
                float v = ( float(j) + drand48() ) / float(ny);
                Ray r = m_camera->getRay(u, v);
                c += color(r, m_world.get(), 0);
            }
            c /= m_samples;
            m_image->write(i, ( ny - j - 1 ), c.getX(), c.getY(), c.getZ());
        }
    }

    stbi_write_bmp(m_filename.c_str(), nx, ny, sizeof(Image::rgb), m_image->pixels());
}
