#pragma once

#include <kazen/common.h>
#include <kazen/vector.h>
#include <cstdint>

NAMESPACE_BEGIN(kazen)

// PMJ02BN Table Declarations
constexpr int nPMJ02bnSets = 5;
constexpr int nPMJ02bnSamples = 65536;
extern const uint32_t pmj02bnSamples[nPMJ02bnSets][nPMJ02bnSamples][2];

// PMJ02BN Table Inline Functions
inline Point2f getPMJ02BNSample(int setIndex, int sampleIndex);

inline Point2f getPMJ02BNSample(int setIndex, int sampleIndex) {
    setIndex %= nPMJ02bnSets;
    // DCHECK_LT(sampleIndex, nPMJ02bnSamples);
    assert(sampleIndex < nPMJ02bnSamples);

    sampleIndex %= nPMJ02bnSamples;

    // Convert from fixed-point.

    // NOTICE: Double precision is key here for the pixel sample sorting, but not
    // necessary otherwise.
    return Point2f(pmj02bnSamples[setIndex][sampleIndex][0] * 0x1p-32,
                   pmj02bnSamples[setIndex][sampleIndex][1] * 0x1p-32);
}

NAMESPACE_END(kazen)