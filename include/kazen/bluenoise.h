#pragma once

#include <kazen/common.h>
#include <kazen/vector.h>

NAMESPACE_BEGIN(kazen)

static constexpr int BlueNoiseResolution = 128;
static constexpr int NumBlueNoiseTextures = 48;

extern const uint16_t BlueNoiseTextures[NumBlueNoiseTextures][BlueNoiseResolution][BlueNoiseResolution];

// Blue noise lookup functions
inline float getBlueNoise(int tableIndex, Point2i p);

inline float getBlueNoise(int textureIndex, Point2i p) {
    // CHECK(textureIndex >= 0 && p.x >= 0 && p.y >= 0);
    assert(textureIndex >= 0 && p.x() >= 0 && p.y() >= 0);

    textureIndex %= NumBlueNoiseTextures;
    int x = p.x() % BlueNoiseResolution, y = p.y() % BlueNoiseResolution;
    return BlueNoiseTextures[textureIndex][x][y] / 65535.f;
}

NAMESPACE_END(kazen)