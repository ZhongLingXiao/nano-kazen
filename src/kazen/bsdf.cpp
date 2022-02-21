#include <kazen/bsdf.h>
#include <kazen/frame.h>
#include <kazen/warp.h>
#include <kazen/texture.h>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/ustring.h>
#include <filesystem/resolver.h>


NAMESPACE_BEGIN(kazen)

/**
 * \brief Diffuse / Lambertian BRDF model
 */
class Diffuse : public BSDF {
public:
    Diffuse(const PropertyList &propList) {
        m_albedo = propList.getColor("albedo", Color3f(0.5f));
    }

    /// Evaluate the BRDF model
    Color3f eval(const BSDFQueryRecord &bRec) const {
        /* This is a smooth BRDF -- return zero if the measure
           is wrong, or when queried for illumination on the backside */
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        /* The BRDF is simply the albedo / pi */
        return m_albedo * INV_PI * Frame::cosTheta(bRec.wo);
    }

    /// Compute the density of \ref sample() wrt. solid angles
    float pdf(const BSDFQueryRecord &bRec) const {
        /* This is a smooth BRDF -- return zero if the measure
           is wrong, or when queried for illumination on the backside */
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return 1.0f;


        /* Importance sampling density wrt. solid angles:
           cos(theta) / pi.

           Note that the directions in 'bRec' are in local coordinates,
           so Frame::cosTheta() actually just returns the 'z' component.
        */
        return INV_PI * Frame::cosTheta(bRec.wo);
    }

    /// Draw a a sample from the BRDF model
    Color3f sample(BSDFQueryRecord &bRec, const Point2f &sample) const {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);

        bRec.measure = ESolidAngle;

        /* Warp a uniformly distributed sample on [0,1]^2
           to a direction on a cosine-weighted hemisphere */
        bRec.wo = Warp::squareToCosineHemisphere(sample);

        /* Relative index of refraction: no change */
        bRec.eta = 1.0f;

        /* eval() / pdf() * cos(theta) = albedo. There
           is no need to call these functions. */
        return m_albedo;
    }

    bool isDiffuse() const {
        return true;
    }

    /// Return a human-readable summary
    std::string toString() const {
        return fmt::format(
            "Diffuse[\n"
            "  albedo = {}\n"
            "]", m_albedo.toString());
    }

    EClassType getClassType() const { return EBSDF; }
private:
    Color3f m_albedo;
};


/**
 * \brief Dielectric / Ideal dielectric BSDF
 */
class Dielectric : public BSDF {
public:
    Dielectric(const PropertyList &propList) {
        /* Interior IOR (default: BK7 borosilicate optical glass) */
        m_intIOR = propList.getFloat("intIOR", 1.5046f);

        /* Exterior IOR (default: air) */
        m_extIOR = propList.getFloat("extIOR", 1.000277f);
    }

    Color3f eval(const BSDFQueryRecord &) const {
        /* Discrete BRDFs always evaluate to zero in kazen */
        return Color3f(0.0f);
    }

    float pdf(const BSDFQueryRecord &) const {
        /* Discrete BRDFs always evaluate to zero in kazen */
        return 1.0f;
    }

    Color3f sample(BSDFQueryRecord &bRec, const Point2f &sample) const {
        bRec.measure = EDiscrete;

        auto cosThetaI = Frame::cosTheta(bRec.wi);
        auto fresnelTerm = fresnel(cosThetaI, m_extIOR, m_intIOR);

        if (sample.x() < fresnelTerm) {
            /* Reflect wo = -wi + 2*dot(wi, n)*n. In this case is much simpler:
            https://www.pbr-book.org/3ed-2018/Reflection_Models/Specular_Reflection_and_Transmission*/
            bRec.wo = Vector3f(-bRec.wi.x(), -bRec.wi.y(), bRec.wi.z());
            bRec.eta = 1.f;
            return Color3f(1.0f);
        } else {
            auto n = Vector3f(0.0f, 0.0f, 1.0f);
            auto factor = m_intIOR / m_extIOR;
            /* inside out */
            if (Frame::cosTheta(bRec.wi) < 0.f) {
                factor = m_extIOR / m_intIOR;
                n.z() = -1.0f;
            }
            bRec.wo = refract(-bRec.wi, n, factor);
            bRec.eta = m_intIOR / m_extIOR;
            return Color3f(1.0f);
        }   
    }

    std::string toString() const {
        return fmt::format(
            "Dielectric[\n"
            "  intIOR = {},\n"
            "  extIOR = {}\n"
            "]",
            m_intIOR, m_extIOR);
    }
private:
    float m_intIOR, m_extIOR;
};


/**
 * \brief Mirror / Ideal mirror BRDF
 */
class Mirror : public BSDF {
public:
    Mirror(const PropertyList &) { }

    Color3f eval(const BSDFQueryRecord &) const {
        /* Discrete BRDFs always evaluate to zero in kazen */
        return Color3f(0.0f);
    }

    float pdf(const BSDFQueryRecord &) const {
        /* Discrete BRDFs always evaluate to zero in kazen */
        return 1.0f;
    }

    Color3f sample(BSDFQueryRecord &bRec, const Point2f &) const {
        if (Frame::cosTheta(bRec.wi) <= 0) 
            return Color3f(0.0f);

        // Reflection in local coordinates
        bRec.wo = Vector3f(
            -bRec.wi.x(),
            -bRec.wi.y(),
             bRec.wi.z()
        );
        bRec.measure = EDiscrete;

        /* Relative index of refraction: no change */
        bRec.eta = 1.0f;

        return Color3f(1.0f);
    }

    std::string toString() const {
        return "Mirror[]";
    }
};


/**
 * \brief Diffuse / Lambertian BRDF model
 */
class Lambertian : public BSDF {
public:
    Lambertian(const PropertyList &propList) {
        auto fileName = propList.getString("albedo");
        filesystem::path filePath = getFileResolver()->resolve(fileName);
        m_albedo = OIIO::ustring(filePath.str());
    }

    /// Evaluate the BRDF model
    Color3f eval(const BSDFQueryRecord &bRec) const {
        /* This is a smooth BRDF -- return zero if the measure
           is wrong, or when queried for illumination on the backside */
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        /* The BRDF is simply the albedo / pi */
        OIIO::TextureOpt options;
        float color[3] = {1.0f, 1.0f, 1.0f};
        getTextureSystem()->texture(
            m_albedo,
            options,
            bRec.uv.x(), bRec.uv.y(),
            0, 0, 0, 0,
            3, &color[0]);  
        return Color3f(color[0], color[1], color[2]).toLinearRGB() * INV_PI * Frame::cosTheta(bRec.wo);
    }

    /// Compute the density of \ref sample() wrt. solid angles
    float pdf(const BSDFQueryRecord &bRec) const {
        /* This is a smooth BRDF -- return zero if the measure
           is wrong, or when queried for illumination on the backside */
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return 1.0f;


        /* Importance sampling density wrt. solid angles:
           cos(theta) / pi.

           Note that the directions in 'bRec' are in local coordinates,
           so Frame::cosTheta() actually just returns the 'z' component.
        */
        return INV_PI * Frame::cosTheta(bRec.wo);
    }

    /// Draw a a sample from the BRDF model
    Color3f sample(BSDFQueryRecord &bRec, const Point2f &sample) const {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);

        bRec.measure = ESolidAngle;

        /* Warp a uniformly distributed sample on [0,1]^2
           to a direction on a cosine-weighted hemisphere */
        bRec.wo = Warp::squareToCosineHemisphere(sample);

        /* Relative index of refraction: no change */
        bRec.eta = 1.0f;

        /* eval() / pdf() * cos(theta) = albedo. There
           is no need to call these functions. */
        OIIO::TextureOpt options;
        float color[3] = {1.0f, 1.0f, 1.0f};
        getTextureSystem()->texture(
            m_albedo,
            options,
            bRec.uv.x(), bRec.uv.y(),
            0, 0, 0, 0,
            3, &color[0]);  
        return Color3f(color[0], color[1], color[2]).toLinearRGB();
    }

    bool isDiffuse() const {
        return true;
    }

    /// Return a human-readable summary
    std::string toString() const {
        return fmt::format(
            "Lambertian[\n"
            "  albedo = {}\n"
            "]", m_albedo.string());
    }

    EClassType getClassType() const { return EBSDF; }
private:
    OIIO::ustring m_albedo;
};


KAZEN_REGISTER_CLASS(Diffuse, "diffuse");
KAZEN_REGISTER_CLASS(Dielectric, "dielectric");
KAZEN_REGISTER_CLASS(Mirror, "mirror");
KAZEN_REGISTER_CLASS(Lambertian, "lambertian");
NAMESPACE_END(kazen)
