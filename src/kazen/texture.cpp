#include <kazen/object.h>
#include <kazen/texture.h>
#include <filesystem/resolver.h>


NAMESPACE_BEGIN(kazen)

class ImageTexture : public Texture<Color3f> {
public:
    ImageTexture(const PropertyList &props) {
        m_name = props.getString("fileName", "");
        // filesystem::path filePath = getFileResolver()->resolve(m_name);
        // TODO: load file here
    }

    Color3f eval(Point2d &uv) {
        return Color3f(0.f);
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
};


KAZEN_REGISTER_CLASS(ImageTexture, "image");
NAMESPACE_END(kazen)