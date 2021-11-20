#pragma once

#include <kazen/object.h>

NAMESPACE_BEGIN(kazen)

/**
 * \brief Load a scene from the specified filename and
 * return its root object
 */
extern Object *loadFromXML(const std::string &filename);

NAMESPACE_END(kazen)
