#include <kazen/object.h>
#include <kazen/texture.h>
#include <OpenImageIO/imageio.h>
#include <filesystem/resolver.h>


NAMESPACE_BEGIN(kazen)

class ImageTexture : public Texture<Color3f> {
public:
    ImageTexture(const PropertyList &props) {
        auto m_fileName = props.getString("filename");
        filesystem::path filePath = getFileResolver()->resolve(m_fileName);
        
        /* Load in image info */
        auto in = OIIO::ImageInput::open(filePath.str());
        if (! in)
            throw Exception("Fail to open texure: {}", filePath.str());
        const OIIO::ImageSpec &spec = in->spec();
        m_width = spec.width;
        m_height = spec.height;
        int channels = spec.nchannels;
        m_data.resize(m_width*m_height*channels);
        in->read_image(OIIO::TypeDesc::UINT8, &m_data[0]);
        in->close ();
    }

    Color3f eval(const Point2f &uv, bool linear=true) const override {
        /* Get delta u and delta v */
        float du = uv.x() * m_width;
        float dv = uv.y() * m_height;
        int u = (int)du;
        int v = (int)dv;

        /* Find the index by multiplying the v for the width. */
        long index = (v * m_width + u)*4;
        index = index % m_data.size(); 

        //http://viclw17.github.io/2019/04/12/raytracing-uv-mapping-and-texturing/
        auto rcp = 1.f / 255.f;
        float r = m_data[index] * rcp;
        float g = m_data[index + 1] * rcp;
        float b = m_data[index + 2] * rcp;

        /* Notice: Needed toLinearRGB to get linear color workflow */
        if (linear)
            return Color3f(r, g, b).toLinearRGB();
        
        return Color3f(r, g, b);
    }

    std::string toString() const {
        return fmt::format(
                "ImageTexture[  \n"
                "  m_filePath = {}},\n"
                "]",
                m_fileName
        );
    }

private:
    std::string m_fileName;
    std::vector<unsigned char> m_data;
    unsigned m_width;
    unsigned m_height;
};


KAZEN_REGISTER_CLASS(ImageTexture, "image");
NAMESPACE_END(kazen)