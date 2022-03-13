#include <kazen/object.h>
#include <kazen/texture.h>
#include <OpenImageIO/imageio.h>
#include <filesystem/resolver.h>


NAMESPACE_BEGIN(kazen)

/// constant 
class ConstantTexture : public Texture<Color3f> {
public:
    ConstantTexture(const PropertyList &props) {
        m_color = props.getColor("color", Color3f(0.5f));
    }

    Color3f eval(const Point2f &uv) const override {
        return m_color;
    }

    std::string toString() const {
        return fmt::format(
                "ConstantTexture[]  \n"
        );        
    }

private:
    Color3f m_color;
};


/// image texture
class ImageTexture : public Texture<Color3f> {
public:
    ImageTexture(const PropertyList &propList) {
        auto fileName = propList.getString("filename");
        filesystem::path filePath = getFileResolver()->resolve(fileName);
        m_filename = OIIO::ustring(filePath.str());     
        auto m_colorspace = propList.getString("colorspace", "srgb");
    }

    Color3f eval(const Point2f &uv) const override {    
        OIIO::TextureOpt options;
        float color[3] = {0.5f, 0.5f, 1.0f};
        getTextureSystem()->texture(
            m_filename,
            options,
            uv.x(), 1.0f-uv.y(),
            0, 0, 0, 0,
            3, &color[0]);  
        
        /* Notice: Needed toLinearRGB to get linear color workflow */
        if (m_colorspace.string() == "srgb")
            return Color3f(color[0], color[1], color[2]).toLinearRGB();
        
        return Color3f(color[0], color[1], color[2]);
    } 

    std::string toString() const {
        return fmt::format(
                "ImageTexture[          \n"
                "   m_filename = {},    \n"
                "   m_colorspace = {},  \n"
                "]                      \n",
                m_filename.string(),
                m_colorspace.string()
        );
    }

private:
    OIIO::ustring m_filename;
    OIIO::ustring m_colorspace;
};


/// normal map
class NormalTexture : public Texture<Color3f> {
public:
    NormalTexture(const PropertyList &propList) {
        auto fileName = propList.getString("filename");
        filesystem::path filePath = getFileResolver()->resolve(fileName);
        m_filename = OIIO::ustring(filePath.str());     
        // auto m_colorspace = propList.getString("colorspace", "linear");
    }

    Color3f eval(const Point2f &uv) const override {    
        OIIO::TextureOpt options;
        float color[3] = {0.5f, 0.5f, 1.0f};
        getTextureSystem()->texture(
            m_filename,
            options,
            uv.x(), 1.0f-uv.y(),
            0, 0, 0, 0,
            3, &color[0]);  
        return Color3f(color[0], color[1], color[2]);
    } 

    std::string toString() const {
        return fmt::format(
                "NormalTexture[  \n"
                "  m_filePath = {}},\n"
                "]",
                m_filename.string()
        );
    }

private:
    OIIO::ustring m_filename;
};


KAZEN_REGISTER_CLASS(ConstantTexture, "constanttexture");
KAZEN_REGISTER_CLASS(ImageTexture, "imagetexture");
KAZEN_REGISTER_CLASS(NormalTexture, "normaltexture");
NAMESPACE_END(kazen)