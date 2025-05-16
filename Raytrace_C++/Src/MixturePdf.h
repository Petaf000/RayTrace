#pragma once

#include "PDF.h"

class MixturePdf : public Pdf {
public:
    MixturePdf(const Pdf* p0, const Pdf* p1) { m_pdfs[0] = p0; m_pdfs[1] = p1; }

    virtual float value(const HitRec& hrec, const Vector3& direction) const override {
        float pdf0_value = m_pdfs[0]->value(hrec, direction);
        float pdf1_value = m_pdfs[1]->value(hrec, direction);
        return 0.5f * pdf0_value + 0.5f * pdf1_value;
    }

    virtual Vector3 generate(const HitRec& hrec) const override {
        if ( drand48() < 0.5f ) {
            return m_pdfs[0]->generate(hrec);
        }
        else {
            return m_pdfs[1]->generate(hrec);
        }
    }

private:
    const Pdf* m_pdfs[2];
};