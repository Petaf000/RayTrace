#pragma once
#include <memory>
#include <iostream>
#include <vector>

#include <vectormath/scalar/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

#include "Util.h"

class Material;
typedef std::shared_ptr<Material> MaterialPtr;

class Shape;
typedef std::shared_ptr<Shape> ShapePtr;

class Texture;
typedef std::shared_ptr<Texture> TexturePtr;
