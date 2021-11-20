#pragma once

#include <kazen/common.h>

NAMESPACE_BEGIN(kazen)
NAMESPACE_BEGIN(renderer)

    void renderSample(const Scene *scene, Sampler *sampler, ImageBlock &block, const Point2f &pixelSample);
    void renderBlock(const Scene *scene, Sampler *sampler, ImageBlock &block);
    void render(Scene *scene, const std::string &filename);

NAMESPACE_END(renderer)
NAMESPACE_END(kazen)