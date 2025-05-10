#pragma once

#include "Material.h"

#include "Ray.h"
#include "HitRec.h"
#include "ScatterRec.h"

#include "Texture.h"

class DiffuseLight : public Material {
public:
    DiffuseLight(const TexturePtr& emit)
        : m_emit(emit) {
    }

    virtual bool scatter(const Ray& r, const HitRec& hrec, ScatterRec& srec) const override {
        return false;
    }

    virtual Vector3 emitted(const Ray& r, const HitRec& hrec) const override {
        return m_emit->value(hrec.u, hrec.v, hrec.p);
    }

private:
    TexturePtr m_emit;
};