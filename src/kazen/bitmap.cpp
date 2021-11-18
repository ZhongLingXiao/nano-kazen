#include <kazen/bitmap.h>
#include <kazen/common.h>
#include <OpenImageIO/imageio.h>

NAMESPACE_BEGIN(kazen)

Bitmap::Bitmap(const std::string &filename) {
    // TODO: load from file
}

void Bitmap::saveEXR(const std::string &filename) {

    const std::string& path = filename + ".exr";
    fmt::print("[{}x{}] Save openexr file to : {} ", cols(), rows(), path);

    const int channels = 3;  // RGB
    float pixels[cols()*rows()*channels];
    std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create(path);
    if (! out)
        return;
    OIIO::ImageSpec spec(cols(), rows(), channels, OIIO::TypeDesc::FLOAT);
    out->open(path, spec);
    out->write_image(OIIO::TypeDesc::FLOAT, pixels);
    out->close();
}

void Bitmap::savePNG(const std::string &filename) {
    
    const std::string& path = filename + ".png";
    fmt::print("[{}x{}] Save png file to : {} ", cols(), rows(), path);

    const int channels = 3;  // RGB
    unsigned char pixels[cols()*rows()*channels];
    std::unique_ptr<OIIO::ImageOutput> out = OIIO::ImageOutput::create(path);
    if (! out)
        return;
    OIIO::ImageSpec spec(cols(), rows(), channels, OIIO::TypeDesc::UINT8);
    out->open(path, spec);
    out->write_image(OIIO::TypeDesc::UINT8, pixels);
    out->close();
}

NAMESPACE_END(kazen)