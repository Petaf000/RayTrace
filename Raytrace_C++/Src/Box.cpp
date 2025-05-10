#include "Box.h"

#include "Ray.h"
#include "HitRec.h"

#include "ShapeList.h"
#include "Rect.h"
#include "FlipNormals.h"


Box::Box(const Vector3& p0, const Vector3& p1, const MaterialPtr& m): m_p0(p0)
, m_p1(p1)
, m_list(std::make_unique<ShapeList>()) {

    ShapeList* l = new ShapeList();
    l->add(std::make_shared<Rect>(
        p0.getX(), p1.getX(), p0.getY(), p1.getY(), p1.getZ(), Rect::kXY, m));
    l->add(std::make_shared<FlipNormals>(std::make_shared<Rect>(
        p0.getX(), p1.getX(), p0.getY(), p1.getY(), p0.getZ(), Rect::kXY, m)));
    l->add(std::make_shared<Rect>(
        p0.getX(), p1.getX(), p0.getZ(), p1.getZ(), p1.getY(), Rect::kXZ, m));
    l->add(std::make_shared<FlipNormals>(std::make_shared<Rect>(
        p0.getX(), p1.getX(), p0.getZ(), p1.getZ(), p0.getY(), Rect::kXZ, m)));
    l->add(std::make_shared<Rect>(
        p0.getY(), p1.getY(), p0.getZ(), p1.getZ(), p1.getX(), Rect::kYZ, m));
    l->add(std::make_shared<FlipNormals>(std::make_shared<Rect>(
        p0.getY(), p1.getY(), p0.getZ(), p1.getZ(), p0.getX(), Rect::kYZ, m)));
    m_list.reset(l);
}

bool Box::hit(const Ray& r, float t0, float t1, HitRec& hrec) const {
    return m_list->hit(r, t0, t1, hrec);
}
