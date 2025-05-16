#pragma once

#include "Ray.h"

class Pdf;

struct ScatterRec {
    Ray	ray;
    Vector3 albedo;
    const Pdf* pdf; // for importance sampling
    bool is_specular;
};