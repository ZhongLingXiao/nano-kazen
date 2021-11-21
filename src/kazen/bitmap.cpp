#include <kazen/bitmap.h>
#include <kazen/common.h>
#include <OpenImageIO/imageio.h>

NAMESPACE_BEGIN(kazen)

Bitmap::Bitmap(const std::string &filename) {
    // TODO: load from file
}

void Bitmap::saveEXR(const std::string &filename) {

    const std::string& path = filename + ".exr";
    LOG("Save file to ==> {}. Resolution: [{}x{}]", path, cols(), rows());

    const int channels = 3;  // RGB
    std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create(path);
    if (! out)
        return;
    OIIO::ImageSpec spec(cols(), rows(), channels, OIIO::TypeDesc::FLOAT);
    out->open(path, spec);
    out->write_image(OIIO::TypeDesc::FLOAT, data());
    out->close();
}

void Bitmap::savePNG(const std::string &filename) {
    
    const std::string& path = filename + ".png";
    LOG("Save file to ==> {}. Resolution: [{}x{}]", path, cols(), rows());

    const int channels = 3;  // RGB
    uint8_t *rgb8 = new uint8_t[channels * cols() * rows()];
    uint8_t *dst = rgb8;
    for (int i = 0; i < rows(); ++i) {
        for (int j = 0; j < cols(); ++j) {
            Color3f tonemapped = coeffRef(i, j).toSRGB();
            dst[0] = (uint8_t) math::clamp(255.f * tonemapped[0], 0.f, 255.f);
            dst[1] = (uint8_t) math::clamp(255.f * tonemapped[1], 0.f, 255.f);
            dst[2] = (uint8_t) math::clamp(255.f * tonemapped[2], 0.f, 255.f);
            dst += 3;
        }
    }

    std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create(path);
    if (! out)
        return;
    OIIO::ImageSpec spec(cols(), rows(), channels, OIIO::TypeDesc::UINT8);
    out->open(path, spec);
    out->write_image(OIIO::TypeDesc::UINT8, rgb8);
    out->close();
    delete[] rgb8;
}

NAMESPACE_END(kazen)