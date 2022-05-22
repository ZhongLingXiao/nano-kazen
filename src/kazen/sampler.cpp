#include <kazen/sampler.h>
#include <kazen/block.h>
#include <kazen/pcg32.h>
#include <kazen/common.h>
#include <kazen/bluenoise.h>
#include <kazen/pmj02table.h>

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

    void prepare( const ImageBlock &block) {
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

    Point2f nextPixel2D() {
        return next2D();
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
    void prepare( const ImageBlock &block) {}
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

    Point2f nextPixel2D() {
        return next2D();
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


/**
 * Correlated Multi-Jittered Sampling
 *
 * Correlated Multi-Jittered Sampling methods is introduced in Pixar's tech memo :cite:`kensler1967correlated`.
 * https://graphics.pixar.com/library/MultiJitteredSampling/paper.pdf
 * 
 * Unlike the previously described stratified sampler, correlated multi-jittered sample patterns produce
 * samples that are well stratified in 2D but also well stratified when projected onto one dimension.
 * This can greatly reduce the variance of a Monte-Carlo estimator when the function to evaluate exhibits
 * more variation along one axis of the sampling domain than the other.
 * 
 * This sampler achieves this by first placing samples in a canonical arrangement that is stratified in
 * both 2D and 1D. It then shuffles the x-coordinate of the samples in every columns and the
 * y-coordinate in every rows. Fortunately, this process doesn't break the 2D and 1D stratification.
 * Kensler's method futher reduces sample clumpiness by correlating the shuffling applied to the
 * columns and the rows.
 */
class Correlated : public Sampler {
public:
    Correlated(const PropertyList &propList) {
        m_seed = (uint64_t) propList.getInteger("seed", 1);
        m_sampleCount = (uint32_t) propList.getInteger("sampleCount", 16);
        m_resolution[1] = sqrt(m_sampleCount);
        m_resolution[0] = (m_sampleCount + m_resolution[1] - 1) / m_resolution[1];

        if (m_sampleCount != (uint32_t)(m_resolution[0]*m_resolution[1]))
            LOG("Sample count rounded up to {}", m_resolution[0]*m_resolution[1]);    
    
        m_sampleCount = m_resolution[0]*m_resolution[1];
    
    }

    virtual ~Correlated() { }

    std::unique_ptr<Sampler> clone() const {
        std::unique_ptr<Correlated> cloned(new Correlated());
        cloned->m_seed              = m_seed;
        cloned->m_random            = m_random;
        cloned->m_sampleCount       = m_sampleCount;
        cloned->m_resolution        = m_resolution;
        return cloned;
    }

    /* No-op for this sampler */
    void prepare( const ImageBlock &block) {}
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
        /* Compute permutation seed for current pixel and dimension */
        uint64_t hash = Hash(m_pixel, m_dimensionIndex, m_seed);

        /* Shuffle the samples order */
        int p = random::permute(m_sampleIndex, m_sampleCount, hash * 0x45fbe943);
        
        /* Add a random perturbation */
        float j = m_random.nextFloat();
        
        ++m_dimensionIndex;
        return (p + j) / m_sampleCount;
    }
    
    Point2f next2D() {
        /* Compute permutation seed for current pixel and dimension */
        uint64_t hash = Hash(m_pixel, m_dimensionIndex, m_seed);

        /* Shuffle the samples order */
        int s = random::permute(m_sampleIndex, m_sampleCount, hash * 0x51633e2d);

        /* Map the index to its 2D cell */
        uint32_t y = s / m_resolution.x();
        uint32_t x = s % m_resolution.x();

        /* Compute offsets to the appropriate substratum within the cell */
        uint32_t sx = random::permute(x, m_resolution.x(), hash * 0x68bc21eb);
        uint32_t sy = random::permute(y, m_resolution.y(), hash * 0x02e5be93);

        /* Add random perturbations on both axis */
        float jx = m_random.nextFloat();
        float jy = m_random.nextFloat();
        m_dimensionIndex += 2;

        return {(x + (sy + jx)/m_resolution.y()) / m_resolution.x(), 
                (y + (sx + jy)/m_resolution.x()) / m_resolution.y()};
    }

    Point2f nextPixel2D() {
        return next2D();
    }

    std::string toString() const {
        return fmt::format("Correlated");
    }

protected:
    Correlated() { }

private:
    pcg32 m_random;
    Point2i m_resolution;
    Point2i m_pixel;
    uint32_t m_permutationSeed;
};


/// pmj02 blue noise
class PMJ02BN : public Sampler {
public:
    PMJ02BN(const PropertyList &propList) {
        m_seed = (uint64_t) propList.getInteger("seed", 1);
        m_sampleCount = (uint32_t) propList.getInteger("sampleCount", 16);

        if (!math::isPowerOf4(m_sampleCount))
            LOG("PMJ02BNSampler results are best with power-of-4 samples per " 
                "pixel (1, 4, 16, 64, ...)");

        /* Get sorted pmj02bn samples for pixel samples */
        if (m_sampleCount > nPMJ02bnSamples)
            // TODO: Add Error message: PMJ02BNSampler only supports up to {nPMJ02bnSamples} samples per pixel
            // TODO: remove this line
            m_sampleCount = nPMJ02bnSamples;

        /* Compute {m_pixelTileSize} for pmj02bn pixel samples and allocate {m_pixelSamples}
           nPixelSamples should always be 65536 */
        m_pixelTileSize = 1 << (math::log4i(nPMJ02bnSamples) - math::log4i(math::roundUpPow4(m_sampleCount)));
        int nPixelSamples = m_pixelTileSize * m_pixelTileSize * m_sampleCount;
        m_pixelSamples = std::make_shared<std::vector<Point2f>>(nPixelSamples);

        /* Loop over pmj02bn samples and associate them with their pixels */
        std::vector<int> nStored(m_pixelTileSize * m_pixelTileSize, 0);
        for (int i = 0; i < nPMJ02bnSamples; ++i) {
            Point2f p = getPMJ02BNSample(0, i);
            p *= m_pixelTileSize;
            int pixelOffset = int(p.x()) + int(p.y()) * m_pixelTileSize;
            if (nStored[pixelOffset] == m_sampleCount) {
                assert(!math::isPowerOf4(m_sampleCount));
                continue;
            }
            int sampleOffset = pixelOffset * m_sampleCount + nStored[pixelOffset];
            assert((*m_pixelSamples)[sampleOffset] == Point2f(0, 0));
            (*m_pixelSamples)[sampleOffset] = p - Point2f(floor(p.x()), floor(p.y()));
            ++nStored[pixelOffset];
        }  

        for (size_t i = 0; i < nStored.size(); ++i)
            assert(nStored[i] == m_sampleCount);
        for ( int c : nStored)
            assert(c == m_sampleCount);
    }

    ~PMJ02BN() { }

    std::unique_ptr<Sampler> clone() const {
        std::unique_ptr<PMJ02BN> cloned(new PMJ02BN());
        cloned->m_sampleCount   = m_sampleCount;
        cloned->m_seed          = m_seed;
        cloned->m_pixelTileSize = m_pixelTileSize;
        cloned->m_pixelSamples  = m_pixelSamples;
        return cloned;
    }

    /* No-op for this sampler */
    void prepare( const ImageBlock &block) {}
    void generate() {}
    void advance() {}

    void generateSample(Point2i p, int sampleIndex, int dimension=0) {
        m_pixel = p;
        m_sampleIndex = sampleIndex;
        m_dimensionIndex = std::max(2, dimension);
    }

    float next1D() {
        /* Find permuted sample index for 1D PMJ02BNSampler sample */
        uint64_t hash = Hash(m_pixel, m_dimensionIndex, m_seed);
        int index = random::permute(m_sampleIndex, m_sampleCount, hash);

        float delta = getBlueNoise(m_dimensionIndex, m_pixel);
        ++m_dimensionIndex;
        return std::min((index + delta) / m_sampleCount, OneMinusEpsilon);
    }

    Point2f next2D() {    
        /* Compute index for 2D pmj02bn sample */
        int index = m_sampleIndex;
        int pmjInstance = m_dimensionIndex / 2;
        if (pmjInstance >= nPMJ02bnSets) {
            // Permute index to be used for pmj02bn sample array
            uint64_t hash = Hash(m_pixel, m_dimensionIndex, m_seed);
            index = random::permute(m_sampleIndex, m_sampleCount, hash);
        }

        /* Return randomized pmj02bn sample for current dimension */
        Point2f u = getPMJ02BNSample(pmjInstance, index);
        
        /* Apply Cranley-Patterson rotation to pmj02bn sample _u_ */
        u += Vector2f(getBlueNoise(m_dimensionIndex, m_pixel), getBlueNoise(m_dimensionIndex+1, m_pixel));
        if (u.x() >= 1)
            u.x() -= 1;
        if (u.y() >= 1)
            u.y() -= 1;

        m_dimensionIndex += 2;
        return {std::min(u.x(), OneMinusEpsilon), std::min(u.y(), OneMinusEpsilon)};    
    }

    Point2f nextPixel2D() {
        int px = m_pixel.x() % m_pixelTileSize, py = m_pixel.y() % m_pixelTileSize;
        int offset = (px + py * m_pixelTileSize) * m_sampleCount;
        return (*m_pixelSamples)[offset + m_sampleIndex];
    }

    std::string toString() const {
        return fmt::format("PMJ02BN");
    }

protected:
    PMJ02BN() { }

private:
    Point2i m_pixel;
    int m_pixelTileSize;
    std::shared_ptr<std::vector<Point2f>> m_pixelSamples;
};

KAZEN_REGISTER_CLASS(Independent, "independent");
KAZEN_REGISTER_CLASS(Stratified, "stratified");
KAZEN_REGISTER_CLASS(Correlated, "correlated");
KAZEN_REGISTER_CLASS(PMJ02BN, "pmj02bn");

NAMESPACE_END(kazen)