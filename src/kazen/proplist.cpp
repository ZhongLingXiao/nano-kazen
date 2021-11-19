#include <kazen/proplist.h>

NAMESPACE_BEGIN(kazen)

#define DEFINE_PROPERTY_ACCESSOR(Type, TypeName, XmlName)                                   \
    void PropertyList::set##TypeName(const std::string &name, const Type &value) {          \
        if (m_properties.find(name) != m_properties.end())                                  \
            cerr << "Property \"" << name <<  "\" was specified multiple times!" << endl;   \
        auto &prop = m_properties[name];                                                    \
        prop.value.XmlName##_value = value;                                                 \
        prop.type = Property::XmlName##_type;                                               \
    }                                                                                       \
                                                                                            \
    Type PropertyList::get##TypeName(const std::string &name) const {                       \
        auto it = m_properties.find(name);                                                  \
        if (it == m_properties.end())                                                       \
            throw Exception("Property '%s' is missing!", name);                             \
        if (it->second.type != Property::XmlName##_type)                                    \
            throw Exception("Property '%s' has the wrong type! "                            \
                "(expected <" #XmlName ">)!", name);                                        \
        return it->second.value.XmlName##_value;                                            \
    }                                                                                       \
                                                                                            \
    Type PropertyList::get##TypeName(const std::string &name, const Type &defVal) const {   \
        auto it = m_properties.find(name);                                                  \
        if (it == m_properties.end())                                                       \
            return defVal;                                                                  \
        if (it->second.type != Property::XmlName##_type)                                    \
            throw Exception("Property '%s' has the wrong type! "                            \
                "(expected <" #XmlName ">)!", name);                                        \
        return it->second.value.XmlName##_value;                                            \
    }

DEFINE_PROPERTY_ACCESSOR(bool, Boolean, boolean)
DEFINE_PROPERTY_ACCESSOR(int, Integer, integer)
DEFINE_PROPERTY_ACCESSOR(float, Float, float)
DEFINE_PROPERTY_ACCESSOR(Color3f, Color, color)
DEFINE_PROPERTY_ACCESSOR(Point3f, Point, point)
DEFINE_PROPERTY_ACCESSOR(Vector3f, Vector, vector)
DEFINE_PROPERTY_ACCESSOR(std::string, String, string)
DEFINE_PROPERTY_ACCESSOR(Transform, Transform, transform)

NAMESPACE_END(kazen)

