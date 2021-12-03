#pragma once

#include <kazen/object.h>

NAMESPACE_BEGIN(kazen)

/**
 * \brief EmitterQueryRecord
 */
struct LightQueryRecord {

    /// Incident direction
    Vector3f wi;
    
    /// Light Sample position
    Point3f p;
    
    /// Shading point position
    Point3f ref;
    
    /// Normal for that light sample position
    Normal3f n;
    
    ///Ray
    Ray3f shadowRay;
    
    /// Measure associated with the sample
    EMeasure measure;

    /// Pdf to ref
    float pdf;
  
    LightQueryRecord(const Point3f &ref) : ref(ref) {}

    LightQueryRecord(const Point3f &ref, const Point3f &p, const Normal3f &n) : ref(ref), p(p), n(n) {
        wi = (p - ref).normalized();
    }
};


/**
 * \brief Superclass of all lights
 */
class Light : public Object {
public:

    virtual ~Light() {}
    
    virtual Color3f eval(const LightQueryRecord &rec) const = 0;
    
    virtual float pdf(Mesh* mesh, const LightQueryRecord &rec) const = 0;
  
    virtual Color3f sample(Mesh *mesh, LightQueryRecord &rec, Sampler *sampler) const = 0;

    virtual Color3f getRadiance() const = 0;

    /**
     * \brief Return the type of object (i.e. Mesh/Light/etc.) 
     * provided by this instance
     * */
    EClassType getClassType() const { return ELight; }
};

NAMESPACE_END(kazen)
