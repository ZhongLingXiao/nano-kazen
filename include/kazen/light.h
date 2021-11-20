#pragma once

#include <kazen/object.h>

NAMESPACE_BEGIN(kazen)

/**
 * \brief Superclass of all lights
 */
class Light : public Object {
public:

    /**
     * \brief Return the type of object (i.e. Mesh/Light/etc.) 
     * provided by this instance
     * */
    EClassType getClassType() const { return ELight; }
};

NAMESPACE_END(kazen)
