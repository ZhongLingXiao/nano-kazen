#include <kazen/sampler.h>
#include <kazen/block.h>
#include <kazen/pcg32.h>

NAMESPACE_BEGIN(kazen)

/**
 * Independent sampling - returns independent uniformly distributed
 * random numbers on <tt>[0, 1)x[0, 1)</tt>.
 *
 * This class is essentially just a wrapper around the pcg32 pseudorandom
 * number generator. For more details on what sample generators do in
 * general, refer to the \ref Sampler class.
 */
class Independent : public Sampler {
public:
    Independent(const PropertyList &propList) {
        m_sampleCount = (uint32_t) propList.getInteger("sampleCount", 1);
    }

    virtual ~Independent() { }

    std::unique_ptr<Sampler> clone() const {
        std::unique_ptr<Independent> cloned(new Independent());
        cloned->m_sampleCount = m_sampleCount;
        cloned->m_random = m_random;
        return cloned;
    }

    void prepare(const ImageBlock &block) {
        m_random.seed(
            block.getOffset().x(),
            block.getOffset().y()
        );
    }

    void generate() { /* No-op for this sampler */ }
    void advance()  { /* No-op for this sampler */ }

    float next1D() {
        return m_random.nextFloat();
    }
    
    Point2f next2D() {
        return Point2f(
            m_random.nextFloat(),
            m_random.nextFloat()
        );
    }

    std::string toString() const {
        return fmt::format("Independent[sampleCount={}]", m_sampleCount);
    }
protected:
    Independent() { }

private:
    pcg32 m_random;
};


/**
 * Stratified Sampling 
 *
 * The stratified sample generator divides the domain into a discrete number of strata and produces
 * a sample within each one of them. This generally leads to less sample clumping when compared to
 * the independent sampler, as well as better convergence.
 */
class Stratified : public Sampler {
public:
    Stratified(const PropertyList &propList) {
        m_sampleCount = (size_t) propList.getInteger("sampleCount", 1);
    
    }

    virtual ~Stratified() { }

    std::unique_ptr<Sampler> clone() const {
        std::unique_ptr<Stratified> cloned(new Stratified());
        cloned->m_sampleCount = m_sampleCount;
        cloned->m_random = m_random;
        return cloned;
    }

    void prepare([[maybe_unused]] const ImageBlock &block) {
        /* No-op for this sampler */
    }

    void generate() { /* No-op for this sampler */ }
    void advance()  { /* No-op for this sampler */ }

    float next1D() {
        return m_random.nextFloat();
    }
    
    Point2f next2D() {
        return Point2f(
            m_random.nextFloat(),
            m_random.nextFloat()
        );
    }


    std::string toString() const {
        return fmt::format("Stratified");
    }

protected:
    Stratified() { }

private:
    pcg32 m_random;
};


KAZEN_REGISTER_CLASS(Independent, "independent");
KAZEN_REGISTER_CLASS(Stratified, "stratified");
NAMESPACE_END(kazen)