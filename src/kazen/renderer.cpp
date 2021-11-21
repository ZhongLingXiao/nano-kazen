#include <kazen/renderer.h>
#include <kazen/parser.h>
#include <kazen/scene.h>
#include <kazen/camera.h>
#include <kazen/block.h>
#include <kazen/timer.h>
#include <kazen/bitmap.h>
#include <kazen/sampler.h>
#include <kazen/progress.h>
#include <kazen/integrator.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/global_control.h>
#include <thread>


NAMESPACE_BEGIN(kazen)
NAMESPACE_BEGIN(renderer)

void renderSample(const Scene *scene, Sampler *sampler, ImageBlock &block, const Point2f &pixelSample) {
    const Integrator *integrator = scene->getIntegrator();
    const Camera *camera = scene->getCamera();
    /* Get random 2d */
    Point2f apertureSample = sampler->next2D();

    /* Sample a ray from the camera */
    Ray3f ray;
    Color3f value = camera->sampleRay(ray, pixelSample, apertureSample);

    /* Compute the incident radiance */
    value *= integrator->Li(scene, sampler, ray);

    /* Store in the image block */
    block.put(pixelSample, value);
}


void renderBlock(const Scene *scene, Sampler *sampler, ImageBlock &block) {
    Point2i offset = block.getOffset();
    Vector2i size  = block.getSize();

    /* Clear the block contents */
    block.clear();

    /* For each pixel and pixel sample sample */
    for (int y=0; y<size.y(); ++y) {
        for (int x=0; x<size.x(); ++x) {
            for (uint32_t i=0; i<sampler->getSampleCount(); ++i) {
                Point2f pixelSample = Point2f((float) (x + offset.x()), (float) (y + offset.y())) + sampler->next2D();
                /* Render all contained pixels */
                renderSample(scene, sampler, block, pixelSample);
            }
        }
    }
}


void render(Scene *scene, const std::string &filename) {
    const Camera *camera = scene->getCamera();
    Vector2i outputSize = camera->getOutputSize();
    scene->getIntegrator()->preprocess(scene);

    /* Create a block generator (i.e. a work scheduler) */
    BlockGenerator blockGenerator(outputSize, KAZEN_BLOCK_SIZE);

    /* Allocate memory for the entire output image and clear it */
    ImageBlock result(outputSize, camera->getReconstructionFilter());
    result.clear();

    /* Do the following in parallel and asynchronously */
    std::thread render_thread([&] {
        auto progress = Progress("Rendering...");
        std::mutex mutex;
        cout.flush();
        Timer timer;

        /* Total number of blocks to be handled, including multiple passes. */
        size_t totalBlocks = blockGenerator.getBlockCount();
        size_t blocksDone = 0;

        tbb::blocked_range<int> range(0, blockGenerator.getBlockCount());

        auto map = [&](const tbb::blocked_range<int> &range) {
            /* Allocate memory for a small image block to be rendered by the current thread */
            ImageBlock block(Vector2i(KAZEN_BLOCK_SIZE),
                camera->getReconstructionFilter());

            /* Create a clone of the sampler for the current thread */
            std::unique_ptr<Sampler> sampler(scene->getSampler()->clone());

            for (int i=range.begin(); i<range.end(); ++i) {
                /* Request an image block from the block generator */
                blockGenerator.next(block);

                /* Inform the sampler about the block to be rendered */
                sampler->prepare(block);

                /* Render all contained blocks */
                renderBlock(scene, sampler.get(), block);

                /* The image block has been processed. Now add it to
                   the "big" block that represents the entire image */
                result.put(block);

                /* Critical section: update progress bar */ {
                    std::lock_guard<std::mutex> lock(mutex);
                    blocksDone++;
                    progress.update(blocksDone / (float)totalBlocks);
                }
            }
        };

        /// Default: parallel rendering
        tbb::parallel_for(range, map);

        /// (equivalent to the following single-threaded call)
        // map(range);

        cout << "done. (took " << timer.elapsedString() << ")" << endl;
    });

    /* Shut down the user interface */
    render_thread.join();

    /* Now turn the rendered image block into
       a properly normalized bitmap */
    std::unique_ptr<Bitmap> bitmap(result.toBitmap());

    /* Determine the filename of the output bitmap */
    std::string outputName = filename;
    size_t lastdot = outputName.find_last_of(".");
    if (lastdot != std::string::npos)
        outputName.erase(lastdot, std::string::npos);

    /* Save using the OpenEXR format */
    bitmap->saveEXR(outputName);

    /* Save tonemapped (sRGB) output using the PNG format */
    bitmap->savePNG(outputName);
}

NAMESPACE_END(renderer)
NAMESPACE_END(kazen)