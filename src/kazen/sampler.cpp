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

    void prepare([[maybe_unused]] const ImageBlock &block) {
        // m_random.seed(
        //     block.getOffset().x(),
        //     block.getOffset().y()
        // );
    }

    /* No-op for this sampler */ 
    void generate() {}
    void advance() {}
    void generateSample(Point2i p, int sampleIndex, int dimension=0) {
        m_random.seed(Hash(p, m_seed));
        m_random.advance(sampleIndex * 65536ull + dimension);
    }

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
        m_seed = (uint64_t) propList.getInteger("seed", 1);
        m_sampleCount = (size_t) propList.getInteger("sampleCount", 16);
        m_resolution = (size_t) propList.getInteger("resolution", 4);
        while (sqr(m_resolution) < m_sampleCount)
            m_resolution++;
        if (m_sampleCount != sqr(m_resolution))
            LOG("Sample count should be square and power of two, rounding to {}", sqr(m_resolution));    
    
        m_sampleCount = sqr(m_resolution);
    }

    virtual ~Stratified() { }

    std::unique_ptr<Sampler> clone() const {
        std::unique_ptr<Stratified> cloned(new Stratified());
        cloned->m_seed              = m_seed;
        cloned->m_random            = m_random;
        cloned->m_sampleCount       = m_sampleCount;
        cloned->m_resolution        = m_resolution;
        return cloned;
    }

    /* No-op for this sampler */
    void prepare([[maybe_unused]] const ImageBlock &block) {}
    void generate() {}
    void advance() {}

    void generateSample(Point2i p, int sampleIndex, int dimension=0) {
        m_pixel = p;
        m_sampleIndex = sampleIndex;
        m_dimensionIndex = dimension;
        m_random.seed(Hash(p, m_seed));
        m_random.advance(sampleIndex * 65536ull + dimension);
    }

    float next1D() {
        /* Compute stratum index for current pixel and dimension */
        uint64_t hash = Hash(m_pixel, m_dimensionIndex, m_seed);
        int stratum = random::permute(m_sampleIndex, m_sampleCount, hash);

        ++m_dimensionIndex;
        float delta = m_random.nextFloat();
        return (stratum + delta) / m_sampleCount;
    }
    
    Point2f next2D() {
        /* Compute stratum index for current pixel and dimension */
        uint64_t hash = Hash(m_pixel, m_dimensionIndex, m_seed);
        int stratum = random::permute(m_sampleIndex, m_sampleCount, hash);

        m_dimensionIndex += 2;
        int x = stratum % m_resolution, y = stratum / m_resolution;
        float dx = m_random.nextFloat();
        float dy = m_random.nextFloat();
        return {(x + dx) / m_resolution, (y + dy) / m_resolution};
    }

    std::string toString() const {
        return fmt::format("Stratified");
    }

protected:
    Stratified() { }

private:
    pcg32 m_random;
    int m_resolution;
    Point2i m_pixel;
};


KAZEN_REGISTER_CLASS(Independent, "independent");
KAZEN_REGISTER_CLASS(Stratified, "stratified");
NAMESPACE_END(kazen)