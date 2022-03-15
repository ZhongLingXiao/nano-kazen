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
                "  m_filename = {},\n"
                "]",
                m_filename.string()
        );
    }

private:
    OIIO::ustring m_filename;
};


/// TODO: remove all the following code

/// color ramp texture
class ColorRampTexture : public Texture<Color3f> {
public:
    ColorRampTexture(const PropertyList &propList) {
        m_min= propList.getFloat("min", 0.0f);
        m_max= propList.getFloat("max", 1.0f);
    }

    ~ColorRampTexture() {
        if (m_nested) delete m_nested;
    }

    inline float ramp(float input) const {
        input = math::clamp(input, 0.0f, 1.0f);
        return m_min + (m_max - m_min) * input;
    }

    Color3f eval(const Point2f &uv) const override { 
        if (m_nested) {
            auto color = m_nested->eval(uv);
            return Color3f(ramp(color.x()), ramp(color.y()), ramp(color.z()));  
        }         
        else return Color3f(0.f);
    } 

    void addChild(Object *obj) override {
        switch (obj->getClassType()) {
            case ETexture:
                m_nested = static_cast<Texture<Color3f>*>(obj);
                break;
            default:
                throw Exception("addChild is not supported other than nested Texture");
        }
    }

    std::string toString() const {
        return fmt::format("ColorRampTexture[]");
    }

private:
    float m_min = 0.0f;
    float m_max = 1.0f;
    Texture<Color3f>* m_nested=nullptr; 
};

/// normal map
class MaskTexture : public Texture<Color3f> {
public:
    MaskTexture(const PropertyList &propList) {
        auto fileName = propList.getString("filename");
        filesystem::path filePath = getFileResolver()->resolve(fileName);
        m_filename = OIIO::ustring(filePath.str());     
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
                "MaskTexture[  \n"
                "  m_filename = {},\n"
                "]",
                m_filename.string()
        );
    }

    ETextureType getTextureType() const override {
        return ETextureType::EMask;
    }

private:
    OIIO::ustring m_filename;
};


/// Mix texture
class BlendTexture : public Texture<Color3f> {
public:
    BlendTexture(const PropertyList &propList) {
        m_op = propList.getString("blendmode", "mix");
        m_nested.reserve(2);
    }

    ~BlendTexture() {
        if (m_mask) delete m_mask;
        for (auto& tex : m_nested) {
            if (tex) delete tex;
        }
    }

    Color3f eval(const Point2f &uv) const override { 
        if (m_nested.size() != 2) {
            throw Exception("Blend: need 2 textures, current size is : {}", m_nested.size());
        }

        Color3f mask = Color3f(0.5);
        if (m_mask)
            mask = m_mask->eval(uv);

        auto color1 = m_nested[0]->eval(uv);
        auto color2 = m_nested[1]->eval(uv);

        if (m_op == "mix") {
            return Color3f( math::lerp(mask.x(), color2.x(), color1.x()),
                            math::lerp(mask.y(), color2.y(), color1.y()),
                            math::lerp(mask.z(), color2.z(), color1.z()));
        }
        else if (m_op == "multiply") {
            return Color3f( color2.x()*color1.x(),
                            color2.y()*color1.y(),
                            color2.z()*color1.z());
        }

        return Color3f(0.f);
    } 

    void addChild(Object *obj) override {
        switch (obj->getClassType()) {
            case ETexture: {
                Texture<Color3f>* temp = static_cast<Texture<Color3f>*>(obj);
                if (temp->getTextureType() == ETextureType::EMask) {
                    m_mask = temp;
                }
                else {
                    m_nested.push_back(temp);
                }
                break;
            }
            default:
                throw Exception("addChild is not supported other than nested Texture");
        }
    }

    std::string toString() const {
        return fmt::format("BlendTexture[]");
    }

private:
    std::string m_op = "mix";
    Texture<Color3f>* m_mask=nullptr; 
    std::vector<Texture<Color3f>* > m_nested; 
};


KAZEN_REGISTER_CLASS(ConstantTexture, "constanttexture");
KAZEN_REGISTER_CLASS(ImageTexture, "imagetexture");
KAZEN_REGISTER_CLASS(NormalTexture, "normaltexture");
KAZEN_REGISTER_CLASS(ColorRampTexture, "colorramp");
KAZEN_REGISTER_CLASS(MaskTexture, "mask");
KAZEN_REGISTER_CLASS(BlendTexture, "blend");

NAMESPACE_END(kazen)