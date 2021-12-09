#include <kazen/object.h>
#include <kazen/texture.h>
#include <OpenImageIO/imageio.h>
#include <filesystem/resolver.h>


NAMESPACE_BEGIN(kazen)

class ImageTexture : public Texture<Color3f> {
public:
    ImageTexture(const PropertyList &props) {
        m_name = props.getString("fileName", "");
        filesystem::path filename = getFileResolver()->resolve(m_name);
        
        /* Load in image info */
        auto in = OIIO::ImageInput::open(filename);
        if (! in)
            throw Exception("Fail to open texure: {}", filename);
        const ImageSpec &spec = in->spec();
        m_width = spec.width;
        m_height = spec.height;
        int channels = spec.nchannels;
        m_data.reserve(m_width*m_height*channels)
        in->read_image (TypeDesc::UINT8, &m_data[0]);
        in->close ();

    }

    Color3f eval(Point2d &uv) {
        /* Get delta u and delta v */
        float du = uv.x() * m_width;
        float dv = uv.y() * m_height;
        int u = (int)du;
        int v = (int)dv;

        /* Find the index by multiplying the v for the width. */
        long index = (v * m_width + u)*4;
        index = index % m_data.size(); 

        //http://viclw17.github.io/2019/04/12/raytracing-uv-mapping-and-texturing/
        float r = m_data[index] / 255.0;
        float g = m_data[index + 1] / 255.0;
        float b = m_data[index + 2] / 255.0;

        return Color3f(r, g, b);
    }

    std::string toString() const {
        return fmt::format(
                "ImageTexture[  \n"
                "  m_name = {}},\n"
                "]",
                m_name
        );
    }

private:
    std::string m_name;
    std::vector<unsigned char> m_data;
    unsigned m_width;
    unsigned m_height;
};


KAZEN_REGISTER_CLASS(ImageTexture, "image");
NAMESPACE_END(kazen)