#include "Scene.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#define NUM_THREAD 4
#define MAX_DEPTH 50

// Objects
#include "ShapeList.h"
//#include "Sphere.h"
//#include "Rect.h"
//#include "FlipNormals.h"
//#include "Box.h"
#include "ShapeBuilder.h"

// Structs
#include "HitRec.h"
#include "ScatterRec.h"

// Materials
#include "Lambertian.h"
#include "Metal.h"
#include "Dielectric.h"
#include "DiffuseLight.h"

// Textures
#include "ColorTexture.h"
#include "CheckerTexture.h"
#include "ImageTexture.h"

void Scene::build() {
    m_backColor = Vector3(0);

    // Camera

    Vector3 lookfrom(278, 278, -800);
    Vector3 lookat(278, 278, 0);
    Vector3 vup(0, 1, 0);
    float aspect = float(m_image->width()) / float(m_image->height());
    m_camera = std::make_unique<Camera>(lookfrom, lookat, vup, 40, aspect);

    // Shapes

    MaterialPtr red = std::make_shared<Lambertian>(
        std::make_shared<ColorTexture>(Vector3(0.65f, 0.05f, 0.05f)));
    MaterialPtr white = std::make_shared<Lambertian>(
        std::make_shared<ColorTexture>(Vector3(0.73f)));
    MaterialPtr green = std::make_shared<Lambertian>(
        std::make_shared<ColorTexture>(Vector3(0.12f, 0.45f, 0.15f)));
    MaterialPtr light = std::make_shared<DiffuseLight>(
        std::make_shared<ColorTexture>(Vector3(15.0f)));

    ShapeList* world = new ShapeList();
    ShapeBuilder builder;
    world->add(builder.rectYZ(0, 555, 0, 555, 555, green).flip().get());
    world->add(builder.rectYZ(0, 555, 0, 555, 0, red).get());
    world->add(builder.rectXZ(213, 343, 227, 332, 554, light).get());
    world->add(builder.rectXZ(0, 555, 0, 555, 555, white).flip().get());
    world->add(builder.rectXZ(0, 555, 0, 555, 0, white).get());
    world->add(builder.rectXY(0, 555, 0, 555, 555, white).flip().get());
    world->add(builder.box(Vector3(0), Vector3(165), white)
        .rotate(Vector3::yAxis(), -18)
        .translate(Vector3(130, 0, 65))
        .get());
    world->add(builder.box(Vector3(0), Vector3(165, 330, 165), white)
        .rotate(Vector3::yAxis(), 15)
        .translate(Vector3(265, 0, 295))
        .get());
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
        Vector3 emitted = hrec.mat->emitted(r, hrec);
        ScatterRec srec;
        if ( depth < MAX_DEPTH && hrec.mat->scatter(r, hrec, srec) ) {
            return emitted + mulPerElem(srec.albedo, color(srec.ray, world, depth + 1));
        }
        else {
            return emitted;
        }
    }
    return background(r.direction());
}

void Scene::render() {
    build();

    int nx = m_image->width();
    int ny = m_image->height();
#pragma omp parallel for collapse(3) schedule(dynamic, 1) num_threads(NUM_THREAD)
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
