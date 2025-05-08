#pragma once

#include "Ray.h"

struct ScatterRec {
    Ray	ray;
    Vector3 albedo;
};