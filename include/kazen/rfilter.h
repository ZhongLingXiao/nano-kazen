#pragma once

#include <kazen/object.h>

/// Reconstruction filters will be tabulated at this resolution
#define KAZEN_FILTER_RESOLUTION 32

NAMESPACE_BEGIN(kazen)

/**
 * \brief Generic radially symmetric image reconstruction filter
 *
 * When adding radiance-valued samples to the rendered image, kazen
 * first convolves them with a so-called image reconstruction filter.
 *
 * To learn more about reconstruction filters and sampling theory
 * in general, take a look at the excellenent chapter 7 of PBRT,
 * which is freely available at:
 *
 * http://graphics.stanford.edu/~mmp/chapters/pbrt_chapter7.pdf
 */
class ReconstructionFilter : public Object {
public:
    /// Return the filter radius in fractional pixels
    float getRadius() const { return m_radius; }

    /// Evaluate the filter function
    virtual float eval(float x) const = 0;

    /**
     * \brief Return the type of object (i.e. Mesh/Camera/etc.) 
     * provided by this instance
     * */
    EClassType getClassType() const { return EReconstructionFilter; }
protected:
    float m_radius;
};

NAMESPACE_END(kazen)
