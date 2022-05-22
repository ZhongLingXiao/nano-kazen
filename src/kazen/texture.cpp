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

    Color3f eval( const Point2f &uv) const override {
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
        m_colorspace = propList.getString("colorspace", "srgb");
        m_scale = propList.getFloat("scale", 1.0f);
        m_options.swrap = OIIO::TextureOpt::WrapPeriodic;
        m_options.twrap = OIIO::TextureOpt::WrapPeriodic;
    }

    Color3f eval(const Point2f &uv) const override {    
        float color[3] = {0.5f, 0.5f, 1.0f};
        getTextureSystem()->texture(
            m_filename,
            m_options,
            uv.x()*m_scale, (1.0f-uv.y())*m_scale,
            0, 0, 0, 0,
            3, &color[0]);  
        
        /* Notice: Needed toLinearRGB to get linear color workflow */
        if (m_colorspace == "srgb")
            return Color3f(color[0], color[1], color[2]).toLinearRGB();
        
        return Color3f(color[0], color[1], color[2]);
    } 

    Color3f eval(const Vector3f &dir_) const override {         
        float color[3] = {0.0f, 0.0f, 0.0f};
        Imath::V3f dir(dir_.x(), dir_.y(), dir_.z());
        
        getTextureSystem()->environment(
            m_filename,
            m_options,
            dir,
            Imath::V3f(0.f), Imath::V3f(0.f),
            3, &color[0]);  
        
        return Color3f(color[0], color[1], color[2]);
    }

    std::string toString() const {
        return fmt::format(
                "ImageTexture[          \n"
                "   m_filename = {},    \n"
                "   m_colorspace = {},  \n"
                "]                      \n",
                m_filename.string(),
                m_colorspace
        );
    }

private:
    OIIO::ustring m_filename;
    OIIO::TextureOpt m_options;
    std::string m_colorspace;
    float m_scale;
};


/// TODO: remove all the following code

/// background texture
class BackgroundTexture : public Texture<Color3f> {
public:
    BackgroundTexture(const PropertyList &propList) {
        m_intensity= propList.getFloat("intensity", 1.0f);
    }

    ~BackgroundTexture() {
        if (m_nested) delete m_nested;
    }

    Color3f eval(const Point2f &uv) const override { 
        if (m_nested) {
            return m_intensity * m_nested->eval(uv);  
        }         
        return Color3f(0.f);
    } 

    Color3f eval(const Vector3f &dir) const override { 
        if (m_nested) {
            return m_intensity * m_nested->eval(dir);  
        }         
        return Color3f(0.f);
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
        return fmt::format("Background[]");
    }

private:
    float m_intensity;
    Texture<Color3f>* m_nested = nullptr; 
};


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


/// Mix texture
class BlendTexture : public Texture<Color3f> {
public:
    BlendTexture(const PropertyList &propList) {
        m_blendmode = propList.getString("blendmode", "mix");
    }

    ~BlendTexture() {
        if (m_mask) delete m_mask;
        if (m_input1) delete m_input1;
        if (m_input2) delete m_input2;
    }

    Color3f eval(const Point2f &uv) const override { 

        Color3f mask = Color3f(0.5f);
        Color3f input1 = Color3f(0.f);
        Color3f input2 = Color3f(1.f);
        if (m_mask)
            mask = m_mask->eval(uv);
        if (m_input1)
            input1 = m_input1->eval(uv);
        if (m_input2)
            input2 = m_input2->eval(uv);

        if (m_blendmode == "mix") {
            return Color3f( math::lerp(mask.x(), input1.x(), input2.x()),
                            math::lerp(mask.x(), input1.y(), input2.y()),
                            math::lerp(mask.x(), input1.z(), input2.z()));
        }
        else if (m_blendmode == "multiply") {        
            return Color3f( input1.x()*input2.x(),
                            input1.y()*input2.y(),
                            input1.z()*input2.z());
        }

        return Color3f(0.f);
    } 

    void addChild(Object *obj) override {
        switch (obj->getClassType()) {
            case ETexture: {
                if( obj->getId() == "mask" ) {
                    if (m_mask)
                        throw Exception("There is already an mask defined!");
                    m_mask = static_cast<Texture<Color3f> *>(obj);
                }
                else if ( obj->getId() == "input1" ) {
                    if (m_input1)
                        throw Exception("There is already an input1 defined!");
                    m_input1 = static_cast<Texture<Color3f> *>(obj);
                }
                else if ( obj->getId() == "input2" ) {
                    if (m_input2)
                        throw Exception("There is already an input2 defined!");
                    m_input2 = static_cast<Texture<Color3f> *>(obj);
                } 
                else {
                    throw Exception("The name of this texture does not match any field!");
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
    std::string m_blendmode = "mix";
    Texture<Color3f>* m_mask=nullptr; 
    Texture<Color3f>* m_input1=nullptr; 
    Texture<Color3f>* m_input2=nullptr; 
};


KAZEN_REGISTER_CLASS(ConstantTexture, "constanttexture");
KAZEN_REGISTER_CLASS(ImageTexture, "imagetexture");
KAZEN_REGISTER_CLASS(BackgroundTexture, "background");
KAZEN_REGISTER_CLASS(ColorRampTexture, "colorramp");
KAZEN_REGISTER_CLASS(BlendTexture, "blend");

NAMESPACE_END(kazen)