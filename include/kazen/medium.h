#pragma once

#include <kazen/object.h>

NAMESPACE_BEGIN(kazen)
class Medium : public Object {
public:

    virtual float distance(const Ray3f &ray, const Point2f &sample) const = 0;
    virtual Color3f transmission(float distance) const = 0;

    virtual std::string toString() const = 0;
    EClassType getClassType() const { return EMedium; }
};

NAMESPACE_END(kazen)