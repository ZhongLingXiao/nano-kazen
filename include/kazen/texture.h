#pragma once

#include <kazen/object.h>

NAMESPACE_BEGIN(kazen)

template<typename T>
class Texture : public Object {
public:

    virtual Color3f eval(const Point2f &uv) const = 0;

    /**
     * \brief Return the type of object (i.e. Mesh/BSDF/etc.)
     * provided by this instance
     * */
    EClassType getClassType() const { return ETexture; }

};


NAMESPACE_END(kazen)