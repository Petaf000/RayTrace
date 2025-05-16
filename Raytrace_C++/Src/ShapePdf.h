#pragma once

#include "Shape.h"
#include "PDF.h"

class ShapePdf : public Pdf {
public:
	ShapePdf(const Shape* p, const Vector3& origin) : m_ptr(p), m_origin(origin) {
	}

	virtual float value(const HitRec& hrec, const Vector3& direction) const override {
		return m_ptr->pdf_value(m_origin, direction);
	}

	virtual Vector3 generate(const HitRec& hrec) const override {
		return m_ptr->random(m_origin);
	}

private:
	const Shape* m_ptr;
	Vector3 m_origin;
};