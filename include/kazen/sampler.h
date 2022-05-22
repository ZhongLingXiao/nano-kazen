#pragma once

#include <kazen/object.h>
#include <memory>

NAMESPACE_BEGIN(kazen)

class ImageBlock;

/**
 * \brief Abstract sample generator
 *
 * A sample generator is responsible for generating the random number stream
 * that will be passed an \ref Integrator implementation as it computes the
 * radiance incident along a specified ray.
 *
 * The most simple conceivable sample generator is just a wrapper around the
 * Mersenne-Twister random number generator and is implemented in
 * <tt>independent.cpp</tt> (it is named this way because it generates 
 * statistically independent random numbers).
 *
 * Fancier samplers might use stratification or low-discrepancy sequences
 * (e.g. Halton, Hammersley, or Sobol point sets) for improved convergence.
 * Another use of this class is in producing intentionally correlated
 * random numbers, e.g. as part of a Metropolis-Hastings integration scheme.
 *
 * The general interface between a sampler and a rendering algorithm is as 
 * follows: Before beginning to render a pixel, the rendering algorithm calls 
 * \ref generate(). The first pixel sample can now be computed, after which
 * \ref advance() needs to be invoked. This repeats until all pixel samples have
 * been exhausted.  While computing a pixel sample, the rendering 
 * algorithm requests (pseudo-) random numbers using the \ref next1D() and
 * \ref next2D() functions.
 *
 * Conceptually, the right way of thinking of this goes as follows:
 * For each sample in a pixel, a sample generator produces a (hypothetical)
 * point in an infinite dimensional random number hypercube. A rendering 
 * algorithm can then request subsequent 1D or 2D components of this point 
 * using the \ref next1D() and \ref next2D() functions. Fancy implementations
 * of this class make certain guarantees about the stratification of the 
 * first n components with respect to the other points that are sampled 
 * within a pixel.
 */
class Sampler : public Object {
public:
    /// Release all memory
    virtual ~Sampler() { }

    /// Create an exact clone of the current instance
    virtual std::unique_ptr<Sampler> clone() const = 0;

    /**
     * \brief Prepare to render a new image block
     * 
     * This function is called when the sampler begins rendering
     * a new image block. This can be used to deterministically
     * initialize the sampler so that repeated program runs
     * always create the same image.
     */
    virtual void prepare(const ImageBlock &block) = 0;

    /**
     * \brief Prepare to generate new samples
     * 
     * This function is called initially and every time the 
     * integrator starts rendering a new pixel.
     */
    virtual void generate() = 0;

    /**
     * \brief Prepare to a new pixel sample
     * 
     * This function is called initially and every time the 
     * integrator starts rendering a new pixel sample.
     */
    virtual void generateSample(Point2i p, int sampleIndex, int dimension=0) = 0;
    
    /// Advance to the next sample
    virtual void advance() = 0;

    /// Retrieve the next component value from the current sample
    virtual float next1D() = 0;

    /// Retrieve the next two component values from the current sample
    virtual Point2f next2D() = 0;

    /// Retrieve the next two component values from the current sample
    virtual Point2f nextPixel2D() = 0;

    /// Return the number of configured pixel samples
    virtual uint32_t getSampleCount() const { return m_sampleCount; }

    /**
     * \brief Return the type of object (i.e. Mesh/Sampler/etc.) 
     * provided by this instance
     * */
    EClassType getClassType() const { return ESampler; }
protected:
    /// Base seed value
    uint64_t m_seed;
    /// Number of samples per pixel
    uint32_t m_sampleCount;
    /// Index of the current sample in the sequence
    uint32_t m_sampleIndex;
    /// Index of the current dimension in the sample
    uint32_t m_dimensionIndex;
};

NAMESPACE_END(kazen)
