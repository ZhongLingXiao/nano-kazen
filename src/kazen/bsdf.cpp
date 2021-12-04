#include <kazen/bsdf.h>
#include <kazen/frame.h>
#include <kazen/warp.h>

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
        return m_albedo * INV_PI;
    }

    /// Compute the density of \ref sample() wrt. solid angles
    float pdf(const BSDFQueryRecord &bRec) const {
        /* This is a smooth BRDF -- return zero if the measure
           is wrong, or when queried for illumination on the backside */
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return 0.0f;


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
        return 0.0f;
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
        return 0.0f;
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
 * \brief Microfacet / Microfacet BRDF model
 */
class Microfacet : public BSDF {
public:
    Microfacet(const PropertyList &propList) {
        /* RMS surface roughness */
        m_alpha = propList.getFloat("alpha", 0.1f);

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

    /// Evaluate the BRDF for the given pair of directions
    Color3f eval(const BSDFQueryRecord &bRec) const {
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        auto cosThetaI = Frame::cosTheta(bRec.wi);
        auto cosThetaO = Frame::cosTheta(bRec.wo);
        auto cosThetaH = Frame::cosTheta(wh);
        /*
        Note that this definition is slightly different from the one shown in class: 
        the density includes an extra cosine factor(cosThetaH) for normalization purposes so that 
        it integrates to one over the hemisphere.
        */
        auto d = Warp::squareToBeckmannPdf(wh, m_alpha);
        auto g = G1(bRec.wi, wh) * G1(bRec.wo, wh);
        auto f = fresnel(wh.dot(bRec.wi), m_extIOR, m_intIOR);
        return m_kd / M_PI + m_ks * ((d * f * g) / (4.0f * cosThetaI * cosThetaO * cosThetaH));
    }

    /// Evaluate the sampling density of \ref sample() wrt. solid angles
    float pdf(const BSDFQueryRecord &bRec) const {
        if (bRec.wo.z() <= 0) {
            return 0;
        }

        Vector3f wh = (bRec.wi + bRec.wo); wh.normalize();
        float jacobian = 1.f/(4*(wh.dot(bRec.wo)));
        
        float a = m_ks*Warp::squareToBeckmannPdf(wh, m_alpha) * jacobian;
        float b = (1 - m_ks) * bRec.wo.z() * INV_PI;
        
        return a + b;
    }

    /// Sample the BRDF
    Color3f sample(BSDFQueryRecord &bRec, const Point2f &_sample) const {
        if (Frame::cosTheta(bRec.wi) <= 0.f) {
            return Color3f(0.f);
        }

        if(_sample.x() < m_ks) { //specular
            auto sample = _sample.x() / m_ks;
            const Vector3f n = Warp::squareToBeckmann(Point2f(sample, _sample.y()), m_alpha);
            bRec.wo = reflect(bRec.wi, n);
            bRec.measure = ESolidAngle;
        } else { // diffuse
            auto sample = (_sample.x() - m_ks) / (1.f - m_ks);
            bRec.wo = Warp::squareToCosineHemisphere(Point2f(sample, _sample.y()));
            bRec.measure = ESolidAngle;
        }

        if (bRec.wo.z() < 0.f) {
            return Color3f(0.f);
        }
        // Note: Once you have implemented the part that computes the scattered
        // direction, the last part of this function should simply return the
        // BRDF value divided by the solid angle density and multiplied by the
        // cosine factor from the reflection equation, i.e.
        // return eval(bRec) * Frame::cosTheta(bRec.wo) / pdf(bRec);
        Color3f eval_ = eval(bRec);
        if((eval_.array() == 0.f).all()) // fast check
            return Color3f(0.f);

        return eval_ * Frame::cosTheta(bRec.wo) / pdf(bRec);
    }


    float G1(const Vector3f &wv, const Vector3f &wh) const {
        auto c = wv.dot(wh)/Frame::cosTheta(wv);
        if(c<0.f)
            return 0.f; 

        float b = 1.f/(m_alpha * Frame::tanTheta(wv));
        float b2 = b * b;
        if(b < 1.6) {
            return (3.535*b + 2.181*b2)/(1+2.276*b + 2.577 * b2);
        } else {
            return 1;
        }
    }

    bool isDiffuse() const {
        /* While microfacet BRDFs are not perfectly diffuse, they can be
           handled by sampling techniques for diffuse/non-specular materials,
           hence we return true here */
        return true;
    }

    std::string toString() const {
        return fmt::format(
            "Microfacet[\n"
            "  alpha = {},\n"
            "  intIOR = {},\n"
            "  extIOR = {},\n"
            "  kd = {},\n"
            "  ks = {}\n"
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

KAZEN_REGISTER_CLASS(Diffuse, "diffuse");
KAZEN_REGISTER_CLASS(Dielectric, "dielectric");
KAZEN_REGISTER_CLASS(Mirror, "mirror");
KAZEN_REGISTER_CLASS(Microfacet, "microfacet");
NAMESPACE_END(kazen)
