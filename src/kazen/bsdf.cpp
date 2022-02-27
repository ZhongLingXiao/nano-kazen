#include <kazen/bsdf.h>
#include <kazen/frame.h>
#include <kazen/warp.h>
#include <kazen/texture.h>
#include <kazen/mircofacet.h>
#include <Eigen/Geometry> // cross()
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


class GGX : public BSDF {
public:
    GGX(const PropertyList &propList) { 
        m_albedo = propList.getColor("albedo", Color3f(0.5f));
        m_roughness = propList.getFloat("roughness", 0.5f);
        m_anisotropy = propList.getFloat("anisotropy", 0.f);
    }

    Color3f eval(const BSDFQueryRecord &bRec) const {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);        
        Color3f F;
        return evaluateGGXSmithBRDF(bRec.wi, bRec.wo, m_albedo, m_roughness, m_anisotropy, F) * Frame::cosTheta(bRec.wo);
    }

    float pdf(const BSDFQueryRecord &bRec) const {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0) 
            return 0.0f;
        
        auto H = (bRec.wi + bRec.wo).normalized();
        auto alpha = roughnessToAlpha(m_roughness, m_anisotropy);
        auto denom = 4.0f * bRec.wi.dot(H);
        return computeGGXSmithPDF(bRec.wi, H, alpha) / denom;
    }

    Color3f sample(BSDFQueryRecord &bRec, const Point2f &sample) const {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);
        Color3f color = sampleGGXSmithBRDF(bRec.wi, m_albedo, m_roughness, m_anisotropy, sample, bRec.wo, bRec.pdf);
        if (Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        return color * Frame::cosTheta(bRec.wo) / bRec.pdf;

    }

    std::string toString() const {
        return "GGX[]";
    }

private:
    Color3f m_albedo;
    float m_roughness;
    float m_anisotropy;
};


class RoughConductor : public BSDF {
public:
    RoughConductor(const PropertyList &propList) {
        /* RMS surface roughness */
        auto roughness = propList.getFloat("alpha", 0.1f);
        // Specify a minimum alpha value, as the GGX NDF does not support a zero alpha.
        float MIN_ALPHA = 0.001f;
        // Square the roughness value and combine with anisotropy to produce X and Y alpha values.
        m_alpha = std::max(MIN_ALPHA, sqr(roughness));
        
        /* Eta and K (default: gold) */
        std::string material = propList.getString("material","Au");
        if (material == "Au") {
            m_eta = Color3f(0.1431189557f, 0.3749570432f, 1.4424785571f);
            m_k = Color3f(3.9831604247f, 2.3857207478f, 1.6032152899f);
        } else if (material == "Cu") {
            m_eta = Color3f(0.2004376970f, 0.9240334304f, 1.1022119527f);
            m_k = Color3f(3.9129485033f, 2.4528477015f, 2.1421879552f);
        } else if (material == "Cr") {
            m_eta = Color3f(4.3696828663f, 2.9167024892f, 1.6547005413f);
            m_k = Color3f(5.2064337956f, 4.2313645277f, 3.7549467933f);
        }
    }

    /// Evaluate the fresnel for conductor
    Color3f fresnelCond(float cosThetaI, Color3f eta, Color3f k) const {
        Color3f tmp_f = pow(eta,2) + pow(k,2);
        Color3f tmp = tmp_f * pow(cosThetaI,2);
        Color3f Rparl2 = (tmp - (2.f * eta * cosThetaI) + 1) /
                    (tmp + (2.f * eta * cosThetaI) + 1);
        Color3f Rperp2 = (tmp_f - (2.f * eta * cosThetaI) + pow(cosThetaI,2)) /
                    (tmp_f + (2.f * eta * cosThetaI) + pow(cosThetaI,2));
        return (Rparl2 + Rperp2) / 2.0f;
    }

    /// Evaluate the microfacet normal distribution D
    float evalBeckmann(const Normal3f &m) const {
        float temp = Frame::tanTheta(m) / m_alpha,
              ct = Frame::cosTheta(m), ct2 = ct*ct;

        return std::exp(-temp*temp) 
            / (M_PI * m_alpha * m_alpha * ct2 * ct2);
    }

    /// Evaluate Smith's shadowing-masking function G1 
    float smithBeckmannG1(const Vector3f &v, const Normal3f &m) const {
        float tanTheta = Frame::tanTheta(v);

        /* Perpendicular incidence -- no shadowing/masking */
        if (tanTheta == 0.0f)
            return 1.0f;

        /* Can't see the back side from the front and vice versa */
        if (m.dot(v) * Frame::cosTheta(v) <= 0)
            return 0.0f;

        float a = 1.0f / (m_alpha * tanTheta);
        if (a >= 1.6f)
            return 1.0f;
        float a2 = a * a;

        /* Use a fast and accurate (<0.35% rel. error) rational
           approximation to the shadowing-masking function */
        return (3.535f * a + 2.181f * a2) 
             / (1.0f + 2.276f * a + 2.577f * a2);
    }

    /// Evaluate the BRDF for the given pair of directions
    virtual Color3f eval(const BSDFQueryRecord &bRec) const override {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);          
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        
        Color3f F = fresnelCond(wh.dot(bRec.wo), m_eta, m_k);
        float D = evalBeckmann(wh);
        float G = smithBeckmannG1(bRec.wi, wh) * smithBeckmannG1(bRec.wo,wh);
        
        return D * F * G / (4.f * Frame::cosTheta(bRec.wi));
    }

    /// Evaluate the sampling density of \ref sample() wrt. solid angles
    virtual float pdf(const BSDFQueryRecord &bRec) const override {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0) 
            return 0.0f;      
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        float D = evalBeckmann(wh);
        float Jh = 1.f / (4.f * wh.dot(bRec.wo));
        return D * Frame::cosTheta(wh) * Jh;
    }

    /// Sample the BRDF
    virtual Color3f sample(BSDFQueryRecord &bRec, const Point2f &_sample) const override {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);
        
        Point2f sample = _sample;
        Vector3f wh = Warp::squareToBeckmann(sample, m_alpha);
        bRec.wo = reflect(bRec.wi, wh).normalized();
        if (Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        return eval(bRec) / pdf(bRec);
    }

    virtual std::string toString() const override {
        return fmt::format(
            "RoughConductor[\n"
            "  alpha = %f,\n"
            "  intIOR = %f,\n"
            "  extIOR = %f,\n"
            "]",
            m_alpha,
            m_eta,
            m_k
        );
    }
private:
    float m_alpha;
    Color3f m_eta, m_k;
};


class RoughPlastic : public BSDF {
public:
    RoughPlastic(const PropertyList &propList) {
        /* RMS surface roughness */
        auto roughness = propList.getFloat("alpha", 0.1f);
        // Specify a minimum alpha value, as the GGX NDF does not support a zero alpha.
        float MIN_ALPHA = 0.001f;
        // Square the roughness value and combine with anisotropy to produce X and Y alpha values.
        m_alpha = std::max(MIN_ALPHA, sqr(roughness));

        /* Interior IOR (default: BK7 borosilicate optical glass) */
        m_intIOR = propList.getFloat("intIOR", 1.5046f);

        /* Exterior IOR (default: air) */
        m_extIOR = propList.getFloat("extIOR", 1.000277f);

        /* Albedo of the diffuse base material (a.k.a "kd") */
        m_kd = propList.getColor("kd", Color3f(0.5f));

        /* To ensure energy conservation, we must scale the 
           specular component by 1-kd. 
           While that is not a particularly realistic model of what 
           happens in reality, this will greatly simplify the 
           implementation. Please see the course staff if you're 
           interested in implementing a more realistic version 
           of this BRDF. */
        m_ks = 1 - m_kd.maxCoeff();
    }

    /// Evaluate the microfacet normal distribution D
    float evalBeckmann(const Normal3f &m) const {
        float temp = Frame::tanTheta(m) / m_alpha,
              ct = Frame::cosTheta(m), ct2 = ct*ct;

        return std::exp(-temp*temp) 
            / (M_PI * m_alpha * m_alpha * ct2 * ct2);
    }

    /// Evaluate Smith's shadowing-masking function G1 
    float smithBeckmannG1(const Vector3f &v, const Normal3f &m) const {
        float tanTheta = Frame::tanTheta(v);

        /* Perpendicular inci		return 0.0f;
         * dence -- no shadowing/masking */
        if (tanTheta == 0.0f)
            return 1.0f;

        /* Can't see the back side from the front and vice versa */
        if (m.dot(v) * Frame::cosTheta(v) <= 0)
            return 0.0f;

        float a = 1.0f / (m_alpha * tanTheta);
        if (a >= 1.6f)
            return 1.0f;
        float a2 = a * a;

        /* Use a fast and accurate (<0.35% rel. error) rational
           approximation to the shadowing-masking function */
        return (3.535f * a + 2.181f * a2) 
             / (1.0f + 2.276f * a + 2.577f * a2);
    }

    /// Evaluate the BRDF for the given pair of directions
    virtual Color3f eval(const BSDFQueryRecord &bRec) const override {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f); 		
        
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
		float D = evalBeckmann(wh);
		float F = fresnel((wh.dot(bRec.wo)), m_extIOR, m_intIOR);
		float G = (smithBeckmannG1(bRec.wo, wh) * smithBeckmannG1(bRec.wi, wh));

		return m_kd * INV_PI * Frame::cosTheta(bRec.wo) + m_ks * (D * F * G) 
			/ (4.f * Frame::cosTheta(bRec.wi));

    }

    /// Evaluate the sampling density of \ref sample() wrt. solid angles
    virtual float pdf(const BSDFQueryRecord &bRec) const override {
	    if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return 0.0f; 

        Vector3f wh = (bRec.wi + bRec.wo).normalized();
		float D = evalBeckmann(wh);
		float Jh = 1.f / (4.f * abs(wh.dot(bRec.wo)));

		return m_ks * D * Frame::cosTheta(wh) * Jh + (1- m_ks) * Frame::cosTheta(bRec.wo) * INV_PI;
		// return m_ks * Warp::squareToBeckmannPdf(wh, m_alpha) * Jh + (1 - m_ks) * Warp::squareToCosineHemispherePdf(bRec.wo);
	}

    /// Sample the BRDF
    virtual Color3f sample(BSDFQueryRecord &bRec, const Point2f &_sample) const override {
		if (Frame::cosTheta(bRec.wi) <= 0)
			return Color3f(0.0f);

		if (_sample.x() < m_ks) {
			Point2f sample(_sample.x() / m_ks, _sample.y());
			Vector3f wh = Warp::squareToBeckmann(sample, m_alpha);
			bRec.wo = ((2.f * wh.dot(bRec.wi) * wh) - bRec.wi).normalized();
		} else {
			Point2f sample((_sample.x() - m_ks) / (1 - m_ks), _sample.y());
			bRec.wo = Warp::squareToCosineHemisphere(sample);
		}

		if (Frame::cosTheta(bRec.wo) <= 0)
			return Color3f(0.0f);

		// return eval(bRec) / pdf(bRec) * Frame::cosTheta(bRec.wo);
		return eval(bRec) / pdf(bRec);
	}

    virtual std::string toString() const override {
        return fmt::format(
            "RoughPlastic[\n"
            "  alpha = %f,\n"
            "  intIOR = %f,\n"
            "  extIOR = %f,\n"
            "  kd = %s,\n"
            "  ks = %f\n"
            "]",
            m_alpha,
            m_intIOR,
            m_extIOR,
            m_kd.toString(),
            m_ks
        );
    }
private:
    float m_alpha;
    float m_intIOR, m_extIOR;
    float m_ks;
    Color3f m_kd;
};

/// TODO: Rough Dielectric


// disney bsdf
class Disney : public BSDF {
public:


private:
};


KAZEN_REGISTER_CLASS(Diffuse, "diffuse");
KAZEN_REGISTER_CLASS(Dielectric, "dielectric");
KAZEN_REGISTER_CLASS(Mirror, "mirror");
KAZEN_REGISTER_CLASS(Lambertian, "lambertian");
KAZEN_REGISTER_CLASS(GGX, "ggx");
KAZEN_REGISTER_CLASS(RoughConductor, "roughconductor");
KAZEN_REGISTER_CLASS(RoughPlastic, "roughplastic");
// KAZEN_REGISTER_CLASS(Disney, "disney");
NAMESPACE_END(kazen)
