#include <kazen/bsdf.h>
#include <kazen/mesh.h>
#include <kazen/frame.h>
#include <kazen/warp.h>
#include <kazen/texture.h>
#include <kazen/ggx_brdf.h>
#include <kazen/mircofacet.h>
#include <kazen/proplist.h>
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
            return 0.0f;


        /* Importance sampling density wrt. solid angles:
           cos(theta) / pi.

           Note that the directions in 'bRec' are in local coordinates,
           so Frame::cosTheta() actually just returns the 'z' component.
        */
        return INV_PI * Frame::cosTheta(bRec.wo);
    }

    /// Draw a a sample from the BRDF model
    Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);

        bRec.measure = ESolidAngle;

        /* Warp a uniformly distributed sample on [0,1]^2
           to a direction on a cosine-weighted hemisphere */
        bRec.wo = Warp::squareToCosineHemisphere(sample2);

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

    Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const {
        bRec.measure = EDiscrete;

        auto cosThetaI = Frame::cosTheta(bRec.wi);
        auto fresnelTerm = fresnel(cosThetaI, m_extIOR, m_intIOR);

        if (sample1 < fresnelTerm) {
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

    Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const {
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
 * \brief Diffuse / Lambertian BRDF model with texture as albedo
 */
class Lambertian : public BSDF {
public:
    Lambertian( const PropertyList &propList) { }

    ~Lambertian() {
        if(!m_albedo) delete m_albedo;
    }

    Color3f eval(const BSDFQueryRecord &bRec) const override {
        /* This is a smooth BRDF -- return zero if the measure
           is wrong, or when queried for illumination on the backside */
        if (bRec.measure != ESolidAngle
            || Frame::cosTheta(bRec.wi) <= 0
            || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        /* The BRDF is simply the albedo / pi */
        return m_albedo->eval(bRec.uv) * INV_PI * Frame::cosTheta(bRec.wo);
    }

    float pdf(const BSDFQueryRecord &bRec) const override {
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

    Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const override {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);

        bRec.measure = ESolidAngle;

        /* Warp a uniformly distributed sample2 on [0,1]^2
           to a direction on a cosine-weighted hemisphere */
        bRec.wo = Warp::squareToCosineHemisphere(sample2);

        /* Relative index of refraction: no change */
        bRec.eta = 1.0f;

        /* eval() / pdf() * cos(theta) = albedo. There
           is no need to call these functions. */
        return m_albedo->eval(bRec.uv);
    }

    void addChild(Object *obj) override {
        switch (obj->getClassType()) {
            case ETexture:
                m_albedo = static_cast<Texture<Color3f>*>(obj);
                break;
            default:
                throw Exception("addChild is not supported other than albedi maps");
        }
    }

    EClassType getClassType() const { return EBSDF; }

    std::string toString() const {
        return fmt::format("Lambertian[]");
    }    

private:
    Texture<Color3f>* m_albedo=nullptr;
};



/// normal map (normal switch)
class NormalMap : public BSDF {
public:
    NormalMap( const PropertyList &propList) { }

    ~NormalMap() {
        if(!m_normalMap) delete m_normalMap;
        if(!m_nested) delete m_nested;
    }

    Color3f eval(const BSDFQueryRecord &bRec) const override {
        const Intersection &its = bRec.its;
        Color3f rgb = m_normalMap->eval(its.uv);
        Vector3f n(2*rgb.r()-1, 2*rgb.g()-1, 2*rgb.b()-1);

		if (Frame::cosTheta(bRec.wi) > 0 && Frame::cosTheta(bRec.wo) > 0 && n.dot(bRec.wi) <= 0)
            return m_nested->eval(bRec);

        Intersection perturbed(its);
        perturbed.shFrame = getFrame(its, n.normalized(), bRec.wi);
        
        BSDFQueryRecord perturbedQuery(
            perturbed.toLocal(its.toWorld(bRec.wi)),
            perturbed.toLocal(its.toWorld(bRec.wo)), bRec.measure);
        
		if (Frame::cosTheta(bRec.wo) * Frame::cosTheta(perturbedQuery.wo) <= 0)
			return Color3f(0.0f);

        perturbedQuery.uv = bRec.uv;
        perturbedQuery.measure = bRec.measure;
        perturbedQuery.eta = bRec.eta;
        return m_nested->eval(perturbedQuery);
    }

    float pdf(const BSDFQueryRecord &bRec) const override {
        const Intersection &its = bRec.its;
        Color3f rgb = m_normalMap->eval(its.uv);
        Vector3f n(2*rgb.r()-1, 2*rgb.g()-1, 2*rgb.b()-1);

		if (Frame::cosTheta(bRec.wi) > 0 && Frame::cosTheta(bRec.wo) > 0 && n.dot(bRec.wi) <= 0)
            return m_nested->pdf(bRec); 

        Intersection perturbed(its);
        perturbed.shFrame = getFrame(its, n.normalized(), bRec.wi);
        
        BSDFQueryRecord perturbedQuery(
            perturbed.toLocal(its.toWorld(bRec.wi)),
            perturbed.toLocal(its.toWorld(bRec.wo)), bRec.measure);
        
		if (Frame::cosTheta(bRec.wo) * Frame::cosTheta(perturbedQuery.wo) <= 0)
			return 0.0f;
        
        perturbedQuery.uv = bRec.uv;
        perturbedQuery.measure = bRec.measure;
        perturbedQuery.eta = bRec.eta;
        return m_nested->pdf(perturbedQuery);        
    }

    Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const override {
        const Intersection &its = bRec.its;
        Color3f rgb = m_normalMap->eval(its.uv);
        Vector3f n(2*rgb.r()-1, 2*rgb.g()-1, 2*rgb.b()-1);
    
		if (Frame::cosTheta(bRec.wi) > 0 && n.dot(bRec.wi) <= 0) {
			bRec.eta = 1.0f;
			return m_nested->sample(bRec, sample1, sample2);
		}

		Intersection perturbed(its);
		perturbed.shFrame = getFrame(its, n.normalized(), bRec.wi);
        
        BSDFQueryRecord perturbedQuery(perturbed.toLocal(its.toWorld(bRec.wi)));
        perturbedQuery.uv = its.uv;
        perturbedQuery.measure = bRec.measure;
        perturbedQuery.eta = bRec.eta;
        Color3f result = m_nested->sample(perturbedQuery, sample1, sample2);
        if (!result.isZero()) {
            bRec.wo = its.toLocal(perturbed.toWorld(perturbedQuery.wo));
			bRec.eta = perturbedQuery.eta;
			if (Frame::cosTheta(bRec.wo) * Frame::cosTheta(perturbedQuery.wo) <= 0)
				return Color3f(0.0f);
        }
        return result;
    }

    // https://arxiv.org/abs/1705.01263
    Frame getFrame(const Intersection &its, Vector3f n,  Vector3f wi) const {

        // 1. Naive implementation
        Frame result;
        result.n = its.shFrame.toWorld(n).normalized();
        result.s = (its.dpdu - result.n * result.n.dot(its.dpdu)).normalized();
        result.t = result.n.cross(result.s).normalized();       

        return result;

        // 2. Normalmap switch (Iray way)
        // Frame result;
		// Vector3f r = 2.0f * n.dot(wi) * n - wi;
		// if (Frame::cosTheta(r) <= 0) {
		// 	// pull up normal (see p. 46 https://arxiv.org/abs/1705.01263)
		// 	r = (r - r.dot(wi) * 1.01f * wi).normalized();
		// 	n = (wi + r).normalized();
		// }

		// Frame frame = its.shFrame;
		// result.n = frame.toWorld(n).normalized();
		// result.s = (its.dpdu - result.n * result.n.dot(its.dpdu)).normalized();
		// result.t = result.n.cross(result.s).normalized();

		// return result;
    }

    void addChild(Object *obj) override {
        switch (obj->getClassType()) {
            case ETexture:
                m_normalMap = static_cast<Texture<Color3f>*>(obj);
                break;
            case EBSDF:
                m_nested = static_cast<BSDF*>(obj);
                break;
            default:
                throw Exception("addChild is not supported other than normal maps and nested BSDF");
        }
    }

    EClassType getClassType() const { return EBSDF; }

    std::string toString() const {
        return fmt::format("NormalMap[]");
    }    

    float regularize(const Point2f &uv) const { return m_nested->regularize(uv); }

private:
    Texture<Color3f>* m_normalMap=nullptr;
    BSDF* m_nested = nullptr;
};


// // Eric Heitz's 2017 - Microfacet-based Normal Mapping for Robust Monte Carlo Path Tracing
// class NormalMapMicrofacet : public BSDF {
// public:
//     NormalMapMicrofacet(const PropertyList &propList) {
//         m_albedo = propList.getColor("albedo", Color3f(0.5f));

//     }

//     ~NormalMapMicrofacet() {
//         if(!m_normalMap) delete m_normalMap;
//         if(!m_nested) delete m_nested;
//     }

//     static float pdot(Vector3f a, Vector3f b) {
//         return std::max(0.f, a.dot(b));
//     }

//     static Vector3f wt(Vector3f wp) {
//         return Vector3f(-wp.x(), -wp.y(), 0.f).normalized();
//     }

//     static float G1(Vector3f wp, Vector3f w) {
//         return std::min(1.f, std::max(0.f, Frame::cosTheta(w)) * std::max(0.f, Frame::cosTheta(wp))
//             / (pdot(w, wp) + pdot(w, wt(wp)) * Frame::sinTheta(wp))
//         );
//     }

//     static float lambda_p(Vector3f wp, Vector3f wi) {
//         float i_dot_p = pdot(wp, wi);
//         return i_dot_p / (i_dot_p + pdot(wt(wp), wi) * Frame::sinTheta(wp));
//     }

// 	Color3f eval(const BSDFQueryRecord &bRec) const {
// 		if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
// 			return Color3f(0.0f);

// 		Vector3f wp;
//         Color3f rgb = m_normalMap->eval(bRec.its.uv);
//         wp = Vector3f(2*rgb.r()-1, 2*rgb.g()-1, 2*rgb.b()-1).normalized();

// 		if (Frame::cosTheta(wp) <= 0 || (std::abs(wp.x()) < 1e-6 && std::abs(wp.y()) < 1e-6))
// 			return m_nested->eval(bRec);

// 		Frame frame_wp(bRec.its.toWorld(wp));
// 		Intersection perturbed_wp(bRec.its);
// 		perturbed_wp.geoFrame = frame_wp;
// 		perturbed_wp.shFrame = frame_wp;
// 		perturbed_wp.wi = perturbed_wp.toLocal(bRec.its.toWorld(bRec.wi));

// 		Vector3f wo_wp = perturbed_wp.toLocal(bRec.its.toWorld(bRec.wo));
// 		Vector3f wt_ = bRec.its.toWorld(wt(wp));
// 		Vector3f wo = bRec.its.toWorld(bRec.wo);
// 		Vector3f wi = bRec.its.toWorld(bRec.wi);
// 		Vector3f wo_reflected = (wo - 2.0f * wo.dot(wt_) * wt_).normalized();
// 		float notShadowedWpMirror = 1.f - G1(wp, bRec.its.toLocal(wo_reflected));
// 		wo_reflected = perturbed_wp.toLocal(wo_reflected);
// 		float shadowing = G1(wp, bRec.wo);

// 		Color3f value(0.f);

// 		float lambda_p_ = lambda_p(wp, bRec.wi);

// 		// i -> p -> o
// 		BSDFQueryRecord evalSingleP(perturbed_wp, perturbed_wp.wi, wo_wp);
// 		value += m_nested->eval(evalSingleP) * (lambda_p_ * shadowing);

// 		// i -> p -> t -> o
// 		if (wo.dot(wt_) > 0) {
// 			BSDFQueryRecord evalDoubleP(perturbed_wp, perturbed_wp.wi, wo_reflected);
// 			value += m_nested->eval(evalDoubleP) * (lambda_p_ * notShadowedWpMirror * shadowing);
// 		}

// 		// i -> t -> p -> o
// 		if (wi.dot(wt_) > 0) {
// 			Vector3f wi_reflected = (wi - 2.0f * wi.dot(wt_) * wt_).normalized();
// 			BSDFQueryRecord evalDoubleT(perturbed_wp, perturbed_wp.toLocal(wi_reflected), wo_wp);
// 			value += m_nested->eval(evalDoubleT) * ((1.f - lambda_p_) * shadowing);
// 		}

// 		return value;
//     }

// 	float pdf(const BSDFQueryRecord &bRec) const {
// 		if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
// 			return 0.0f;

// 		Vector3f wp;
//         Color3f rgb = m_normalMap->eval(bRec.its.uv);
//         wp = Vector3f(2*rgb.r()-1, 2*rgb.g()-1, 2*rgb.b()-1).normalized();

// 		if (Frame::cosTheta(wp) <= 0 || (std::abs(wp.x()) < 1e-6 && std::abs(wp.y()) < 1e-6)) {
// 			return m_nested->pdf(bRec);
// 		}

// 		float probability_wp = lambda_p(wp, bRec.wi);
// 		Frame frameWp(bRec.its.toWorld(wp));
// 		Intersection perturbed_wp(bRec.its);
// 		perturbed_wp.geoFrame = frameWp;
// 		perturbed_wp.shFrame = frameWp;
// 		Vector3f wi = bRec.its.toWorld(bRec.wi);
// 		Vector3f wo = bRec.its.toWorld(bRec.wo);
// 		Vector3f wt_ = bRec.its.toWorld(wt(wp));
// 		float pdf = 0.f;
// 		if (probability_wp > 0.f) {
// 			BSDFQueryRecord queryWp(perturbed_wp, perturbed_wp.toLocal(wi), perturbed_wp.toLocal(wo));
// 			pdf += probability_wp * m_nested->pdf(queryWp) * G1(wp, bRec.wo);

// 			if (wo.dot(wt_) > 1e-6) {
// 				Vector3f woReflected = (wo - 2.0f * wo.dot(wt_) * wt_).normalized();
// 				BSDFQueryRecord queryWpt(perturbed_wp, perturbed_wp.toLocal(wi), perturbed_wp.toLocal(woReflected));
// 				pdf += probability_wp * m_nested->pdf(queryWpt)
// 					* (1.f - G1(wp, bRec.its.toLocal(woReflected)));
// 			}
// 		}

// 		if (probability_wp < 1.f && wi.dot(wt_) > 1e-6) {
// 			Vector3f wiReflected = (wi - 2.0f * wi.dot(wt_) * wt_).normalized();
// 			BSDFQueryRecord queryWtp(perturbed_wp, perturbed_wp.toLocal(wiReflected), perturbed_wp.toLocal(wo));
// 			pdf += (1.f - probability_wp) * m_nested->pdf(queryWtp);
// 		}

// 		return pdf;
// 	}    

// 	Color3f sample(BSDFQueryRecord &bRec, const Point2f &sample) const {
// 		if (Frame::cosTheta(bRec.wi) <= 0)
// 			return Color3f(0.0f);

// 		bRec.eta = 1.0f;

// 		Vector3f wp;
//         Color3f rgb = m_normalMap->eval(bRec.its.uv);
//         wp = Vector3f(2*rgb.r()-1, 2*rgb.g()-1, 2*rgb.b()-1).normalized();

// 		if (Frame::cosTheta(wp) <= 0 || (std::abs(wp.x()) < 1e-6 && std::abs(wp.y()) < 1e-6)) {
// 			return m_nested->sample(bRec, sample);
// 		}

// 		Frame frame_wp(bRec.its.toWorld(wp));
// 		Intersection perturbed_wp(bRec.its);
// 		perturbed_wp.geoFrame = frame_wp;
// 		perturbed_wp.shFrame = frame_wp;

// 		Vector3f wt_ = bRec.its.toWorld(wt(wp));

// 		Vector3f wr = -bRec.its.toWorld(bRec.wi);
// 		Color3f energy(1.f);
// 		if (sample.x() < lambda_p(wp, bRec.wi)) {
// 			// sample on wp
// 			perturbed_wp.wi = -perturbed_wp.toLocal(wr);
// 			BSDFQueryRecord query_wp(perturbed_wp, Vector3f(1.f));
// 			energy *= m_nested->sample(query_wp, sample);
// 			// did sampling fail?
// 			if (energy.isZero()) return energy;
// 			wr = perturbed_wp.toWorld(query_wp.wo);
// 			float G1_ = G1(wp, bRec.its.toLocal(wr));

// 			// is the sampled direction shadowed?
// 			if (sample.x() > G1_) {
// 				// reflect on wt
// 				wr = (wr + 2.0f * wt_.dot(-wr) * wt_).normalized();
// 				energy *= G1(wp, bRec.its.toLocal(wr));
// 			}
// 		} else {
// 			// do one reflection if we start at wt
// 			wr = (wr + 2.0f * wt_.dot(-wr) * wt_).normalized();
// 			// sample on wp
// 			perturbed_wp.wi = -perturbed_wp.toLocal(wr);
// 			BSDFQueryRecord query_wp(perturbed_wp, NULL);
// 			energy *= m_nested->sample(query_wp, sample);
// 			// did sampling fail?
// 			if (energy.isZero()) return energy;
// 			wr = perturbed_wp.toWorld(query_wp.wo);
// 			energy *= G1(wp, bRec.its.toLocal(wr));
// 		}
// 		bRec.wo = bRec.its.toLocal(wr);
// 		if (Frame::cosTheta(bRec.wo) <= 0.f) return Color3f(0.f);
// 		return energy;
// 	}

//     void addChild(Object *obj) override {
//         switch (obj->getClassType()) {
//             case ETexture:
//                 m_normalMap = static_cast<Texture<Color3f>*>(obj);
//                 break;
//             case EBSDF:
//                 m_nested = static_cast<BSDF*>(obj);
//                 // m_albedo = Color3f(0.f, 1.0f, 0.f); // this line is for debugging
//                 m_albedo = m_nested->getColor();
//                 break;
//             default:
//                 throw Exception("addChild is not supported other than normal maps and nested BSDF");
//         }
//     }

//     EClassType getClassType() const { return EBSDF; }

//     std::string toString() const {
//         return fmt::format("NormalMapMircofacet[]");
//     } 

// private:
//     Color3f m_albedo;
//     Texture<Color3f>* m_normalMap = nullptr;
//     BSDF* m_nested = nullptr;
    
// };


class GGX : public BSDF {
public:
    GGX(const PropertyList &propList) { 
        m_roughness = propList.getFloat("roughness", 0.5f);
        m_anisotropy = propList.getFloat("anisotropy", 0.f);
    }

    ~GGX() {
        if(!m_albedo) delete m_albedo;
    }

    Color3f eval(const BSDFQueryRecord &bRec) const {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);        
        Color3f F;
        Color3f albedo = m_albedo->eval(bRec.uv);
        return evaluateGGXSmithBRDF(bRec.wi, bRec.wo, albedo, m_roughness, m_anisotropy, F) * Frame::cosTheta(bRec.wo);
    }

    float pdf(const BSDFQueryRecord &bRec) const {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0) 
            return 0.0f;
        
        auto H = (bRec.wi + bRec.wo).normalized();
        auto alpha = roughnessToAlpha(m_roughness, m_anisotropy);
        auto denom = 4.0f * bRec.wi.dot(H);
        return computeGGXSmithPDF(bRec.wi, H, alpha) / denom;
    }

    Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);
        Color3f albedo = m_albedo->eval(bRec.uv);            
        Color3f color = sampleGGXSmithBRDF(bRec.wi, albedo, m_roughness, m_anisotropy, sample2, bRec.wo, bRec.pdf);
        
        if (Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        return color * Frame::cosTheta(bRec.wo) / bRec.pdf;

    }

    void addChild(Object *obj) override {
        switch (obj->getClassType()) {
            case ETexture:
                m_albedo = static_cast<Texture<Color3f>*>(obj);
                break;
            default:
                throw Exception("addChild is not supported other than albedi maps");
        }
    }

    std::string toString() const {
        return "GGX[]";
    }

private:
    Texture<Color3f>* m_albedo=nullptr;
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
    float smithBeckmannG1(const Vector3f &v, const Normal3f &m, float alpha) const {
        /* Ensure consistent orientation (can't see the back
           of the microfacet from the front and vice versa) */        
        if (v.dot(m) * Frame::cosTheta(v) <= 0.0f)
            return 0.0f;

        /* Perpendicular incidence -- no shadowing/masking */
        float tanTheta = std::abs(Frame::tanTheta(v));
        if (tanTheta == 0.0f)
            return 1.0f;

        float a = 1.0f / (alpha * tanTheta);
        if (a >= 1.6f)
            return 1.0f;

        /* Use a fast and accurate (<0.35% rel. error) rational
           approximation to the shadowing-masking function */
        float aSqr = a*a;
        return (3.535f * a + 2.181f * aSqr)
             / (1.0f + 2.276f * a + 2.577f * aSqr);
    }

    /// Evaluate the BRDF for the given pair of directions
    virtual Color3f eval(const BSDFQueryRecord &bRec) const override {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);          
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
        
        Color3f F = fresnelCond(wh.dot(bRec.wo), m_eta, m_k);
        float D = evalBeckmann(wh);
        float G = smithBeckmannG1(bRec.wi, wh, m_alpha) * smithBeckmannG1(bRec.wo, wh, m_alpha);
        
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
    virtual Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const override {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);
        
        Point2f sample = sample2;
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
    float smithBeckmannG1(const Vector3f &v, const Normal3f &m, float alpha) const {
        /* Ensure consistent orientation (can't see the back
           of the microfacet from the front and vice versa) */        
        if (v.dot(m) * Frame::cosTheta(v) <= 0.0f)
            return 0.0f;

        /* Perpendicular incidence -- no shadowing/masking */
        float tanTheta = std::abs(Frame::tanTheta(v));
        if (tanTheta == 0.0f)
            return 1.0f;

        float a = 1.0f / (alpha * tanTheta);
        if (a >= 1.6f)
            return 1.0f;

        /* Use a fast and accurate (<0.35% rel. error) rational
           approximation to the shadowing-masking function */
        float aSqr = a*a;
        return (3.535f * a + 2.181f * aSqr)
             / (1.0f + 2.276f * a + 2.577f * aSqr);
    }

    /// Evaluate the BRDF for the given pair of directions
    virtual Color3f eval(const BSDFQueryRecord &bRec) const override {
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f); 		
        
        Vector3f wh = (bRec.wi + bRec.wo).normalized();
		float D = evalBeckmann(wh);
		float F = fresnel((wh.dot(bRec.wo)), m_extIOR, m_intIOR);
		float G = smithBeckmannG1(bRec.wo, wh, m_alpha) * smithBeckmannG1(bRec.wi, wh, m_alpha);

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
    virtual Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const override {
		if (Frame::cosTheta(bRec.wi) <= 0)
			return Color3f(0.0f);

		if (sample1 < m_ks) {
			Vector3f wh = Warp::squareToBeckmann(sample2, m_alpha);
			bRec.wo = ((2.f * wh.dot(bRec.wi) * wh) - bRec.wi).normalized();
		} else {
			bRec.wo = Warp::squareToCosineHemisphere(sample2);
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
class RoughDielectric : public BSDF {
public:
    RoughDielectric(const PropertyList &propList) {
        /* Interior IOR (default: BK7 borosilicate optical glass) */
        m_intIOR = propList.getFloat("intIOR", 1.5046f);

        /* Exterior IOR (default: air) */
        m_extIOR = propList.getFloat("extIOR", 1.000277f);

        /* RMS surface roughness */
        float roughness = propList.getFloat("roughness", 0.1f);   
        float MIN_ALPHA = 0.001f;
        m_alpha = std::max(MIN_ALPHA, sqr(roughness));
        
        m_eta = m_intIOR / m_extIOR;
        m_invEta = m_extIOR / m_intIOR;  
    }

    /// Evaluate the BRDF for the given pair of directions
    virtual Color3f eval(const BSDFQueryRecord &bRec) const override {
        if (Frame::cosTheta(bRec.wi) == 0)
            return Color3f(0.0f);

        float cosThetaI = Frame::cosTheta(bRec.wi);
        float cosThetaO = Frame::cosTheta(bRec.wo);

        /* Determine the type of interaction */
        bool reflect = cosThetaI * cosThetaO > 0.f;
        
        /* Determine the relative index of refraction */
        float eta = cosThetaI > 0.f ? m_eta : m_invEta;
        float invEta = cosThetaI > 0.f ? m_invEta : m_eta;

        /* Compute the half-vector */
        Vector3f wm;
        if (reflect) {
            wm = (bRec.wi + bRec.wo).normalized();
        } else {
            wm = (bRec.wi + bRec.wo*eta).normalized();
        }

        /* Ensure that the half-vector points into the
           same hemisphere as the macrosurface normal */
        wm *= math::sign(Frame::cosTheta(wm));

        /* Compute the F D G */
		float F = fresnelDielectric(bRec.wi.dot(wm), m_eta);
		float D = evalBeckmann(wm, m_alpha);
		float G = smithBeckmannG1(bRec.wo, wm, m_alpha) * smithBeckmannG1(bRec.wi, wm, m_alpha);

        if (reflect) {
            // float fr = std::abs((F * G * D)/ (4.f * std::abs(cosThetaI)));
            float fr = (F * G * D)/ (4.f * std::abs(cosThetaI));
            return Color3f(fr);
        } else {            
            /* Calculate the total amount of transmission */
            float denom = bRec.wi.dot(wm) + eta * bRec.wo.dot(wm);
            float value = ((1 - F) * D * G * eta * eta * bRec.wi.dot(wm) * bRec.wo.dot(wm)) /
                (cosThetaI * sqr(denom));

            return std::abs(value);
        }
    }

    /// Evaluate the sampling density of \ref sample() wrt. solid angles
    virtual float pdf(const BSDFQueryRecord &bRec) const override {
        float cosThetaI = Frame::cosTheta(bRec.wi);
        float cosThetaO = Frame::cosTheta(bRec.wo);

        /* Determine the type of interaction */
        bool reflect = cosThetaI * cosThetaO > 0.f;

        /* Determine the relative index of refraction */
        float eta = cosThetaI > 0.f ? m_eta : m_invEta;    
    
        Vector3f wm;
        float dwm_dwo;

        if (reflect) {
            wm = (bRec.wi + bRec.wo).normalized();
            /* Jacobian of the half-direction mapping */
            dwm_dwo = 1.0f / (4.0f * bRec.wo.dot(wm));
        } else {
            wm = (bRec.wi + bRec.wo * eta).normalized();
            /* Jacobian of the half-direction mapping */
            float sqrtDenom = bRec.wi.dot(wm) + eta * bRec.wo.dot(wm);
            dwm_dwo = (eta*eta * bRec.wo.dot(wm)) / (sqrtDenom*sqrtDenom);
        }

        /* Ensure that the half-vector points into the
           same hemisphere as the macrosurface normal */
        wm *= math::sign(Frame::cosTheta(wm));

        float F = fresnelDielectric(bRec.wi.dot(wm), m_eta);
        float D = evalBeckmann(wm, m_alpha);

        float prob = D * Frame::cosTheta(wm);
        prob *= reflect ? F : (1-F);

        return std::abs(prob * dwm_dwo);
    }

    /// Sample the BRDF
    virtual Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const override {
        /* Trick by Walter et al.: slightly scale the roughness values to
        reduce importance sampling weights. Not needed for the
        Heitz and D'Eon sampling technique. */
        float alpha = m_alpha * ( 1.2f - 0.2f * std::sqrt(std::abs(Frame::cosTheta(bRec.wi))));
        
        // sample the mirco scale normal
        Vector3f wm = Warp::squareToBeckmann(sample2, alpha);

        // check pdf weather is zero
        // float pdf = evalBeckmann(wm, alpha) * Frame::cosTheta(wm);
        float pdf = Warp::squareToBeckmannPdf(wm, alpha);
        if (pdf == 0.f) return Color3f(0.f);

        float cosThetaT;
        float F = fresnelDielectric(bRec.wi.dot(wm), m_eta, cosThetaT);

        bool sampleReflection = true;
        if(sample1 > F) {
            sampleReflection = false;
        }

        if (sampleReflection) {
            bRec.wo = reflect(bRec.wi, wm);
            bRec.eta = 1.0f;
            /* Side check */
            if (Frame::cosTheta(bRec.wi) * Frame::cosTheta(bRec.wo) <= 0)
                return Color3f(0.0f);
        } else {
            if (cosThetaT == 0)
                return Color3f(0.0f);
            bRec.wo = refract(bRec.wi, wm, m_eta, cosThetaT);
            bRec.eta = cosThetaT < 0.f ? m_eta : m_invEta;

            /* Side check */
            if (Frame::cosTheta(bRec.wi) * Frame::cosTheta(bRec.wo) >= 0)
                return Color3f(0.0f);
        }
        
		float D = evalBeckmann(wm, alpha);
		float G = smithBeckmannG1(bRec.wo, wm, alpha) * smithBeckmannG1(bRec.wi, wm, alpha);

        return std::abs(D * G * bRec.wi.dot(wm) / (pdf * Frame::cosTheta(bRec.wi)));
    }

    /// Evaluate the microfacet normal distribution D
    float evalBeckmann(const Normal3f &m, float alpha) const {
        float temp = Frame::tanTheta(m) / alpha,
              ct = Frame::cosTheta(m), ct2 = ct*ct;

        return std::exp(-temp*temp) 
            / (M_PI * alpha * alpha * ct2 * ct2);
    }

    /// Evaluate Smith's shadowing-masking function G1 
    float smithBeckmannG1(const Vector3f &v, const Normal3f &m, float alpha) const {
        /* Ensure consistent orientation (can't see the back
           of the microfacet from the front and vice versa) */        
        if (v.dot(m) * Frame::cosTheta(v) <= 0.0f)
            return 0.0f;

        /* Perpendicular incidence -- no shadowing/masking */
        float tanTheta = std::abs(Frame::tanTheta(v));
        if (tanTheta == 0.0f)
            return 1.0f;

        float a = 1.0f / (alpha * tanTheta);
        if (a >= 1.6f)
            return 1.0f;

        /* Use a fast and accurate (<0.35% rel. error) rational
           approximation to the shadowing-masking function */
        float aSqr = a*a;
        return (3.535f * a + 2.181f * aSqr)
             / (1.0f + 2.276f * a + 2.577f * aSqr);
    }

    static Vector3f refract(const Vector3f &wi, const Vector3f &n, float eta, float cosThetaT) {
        if (cosThetaT < 0)
            eta = 1.f / eta;

        return n * (wi.dot(n) * eta + cosThetaT) - wi * eta;
    }

    std::string toString() const {
        return fmt::format(
            "RoughDielectric"
        );
    }

private:
    float m_intIOR, m_extIOR;
    float m_eta, m_invEta;
    float m_alpha; 
   
};


/// kazen innercircle standard surface (kiss)
/* References:
* [1] [Physically Based Shading at Disney] https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
* [2] [Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering] https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
* [3] [Simon Kallweit's project report] http://simon-kallweit.me/rendercompo2015/report/
* [4] [Microfacet Models for Refraction through Rough Surfaces] https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
* [5] [Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs] https://jcgt.org/published/0003/02/03/paper.pdf
* [6] [Sampling the GGX Distribution of Visible Normals] https://jcgt.org/published/0007/04/01/paper.pdf
*/
class KazenStandardSurface : public BSDF {
public:
    KazenStandardSurface(const PropertyList &proplist) {
        m_anisotropy = proplist.getFloat("anisotropy", 0.0f);
        m_specular = proplist.getFloat("specular", 0.5f);
        m_specularTint = proplist.getFloat("specularTint", 0.5f); 
        m_clearcoat = proplist.getFloat("clearcoat", 0.0f);
        m_clearcoatRoughness = proplist.getFloat("clearcoatRoughness", 0.5f);               
        m_sheen = proplist.getFloat("sheen", 0.0f);
        m_sheenTint = proplist.getFloat("sheenTint", 0.5);
    }

    ~KazenStandardSurface() {
        if(!m_baseColor) delete m_baseColor;
        if(!m_metallic) delete m_metallic;
        if(!m_roughness) delete m_roughness;
    }

    float schlickWeight(float x) const {
        x = math::clamp(1.f - x, 0.f, 1.f);
        auto x2 = x * x;
        return x2 * x2 * x;
    }

    Color3f lerp(const Color3f& c1, const Color3f& c2, float t) const {
        return (1.f - t) * c1 + t * c2;
    }

    float ior(float specular) const {
        return 2.f/(1.f - std::sqrt(0.08*specular)) - 1.f;
    };

    // [Taming the Shadow Terminator. Chiang.2019] https://www.yiningkarlli.com/projects/shadowterminator/shadow_terminator_v1_1.pdf
    float shadowTerminator(const Vector3f &L,const Vector3f &shNormal, const Vector3f &geoNormal) const {
        float NDotL = std::max(0.0f, shNormal.dot(L));
        float NGeomDotL = std::max(0.0f, geoNormal.dot(L));
        float NGeomDotN = std::max(0.0f, geoNormal.dot(shNormal));
        if (NDotL == 0.0f || NGeomDotL == 0.0f || NGeomDotN == 0.0f) {
            return 0.0f;
        } else {
            float G = NGeomDotL / (NDotL * NGeomDotN);
            if (G <= 1.0f) {
                float smoothTerm = -(G * G * G) + (G * G) + G; // smoothTerm is G' in the math
                return smoothTerm;
            }
        }
        return 1.0f;
    }

    // [Predictable and Targeted Softening of the Shadow Terminator] https://dl.acm.org/doi/pdf/10.1145/3388767.3407371
    float shadowTerminator1(const Vector3f &Ng, const Vector3f &Ns, const Vector3f &wi) const {
        const float alpha = 0.05f;
        float d = math::lerp(Ns.dot(Ng), sin(alpha+0.1), sin(alpha));
        float t = std::max(0.0f, std::min(1.0f, Ng.dot(wi)/d));
        float G = t*t*(3.f - 2.f*t);
        return G;    
    }

    Color3f eval(const BSDFQueryRecord &bRec) const {
        /* This is a smooth BRDF -- return zero if the measure
           is wrong, or when queried for illumination on the backside */
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return Color3f(0.0f);

        Vector3f V = bRec.wi;
        Vector3f L = bRec.wo;
        Vector3f H = (V + L).normalized();

        // color 
        Color3f Cdlin = m_baseColor->eval(bRec.uv);
        auto metallic = m_metallic->eval(bRec.uv).r(); // TODO: single channel?
        // auto roughness = m_roughness->eval(bRec.uv).r();
        // if (bRec.its.regularizeFlag)
        //     roughness = std::min(1.f, roughness+bRec.its.accumulatedRoughness);
        auto roughness = std::min(1.f, m_roughness->eval(bRec.uv).r()+bRec.its.accumulatedRoughness);
        float Cdlum = Cdlin.getLuminance();
        Color3f Ctint = Cdlum > 0.f ? Cdlin/Cdlum : Color3f(1.f);
		Color3f Ctintmix = 0.08f * m_specular * lerp(Color3f(1.f), Ctint, m_specularTint);
		Color3f Cspec0 = lerp(Ctintmix, Cdlin, metallic);

        // diffuse
        float FL = schlickWeight(L.z());
        float FV = schlickWeight(V.z());
        float FH = schlickWeight(L.dot(H));

        float cosThetaD = V.dot(H);
         float FD90 = 0.5f * 2 * roughness * cosThetaD * cosThetaD;

        float Lambert = (1.f - 0.5f*FL) * (1.f - 0.5f*FV);
        float RR = 2.f * roughness * cosThetaD * cosThetaD;
        float retro_reflection = RR * (FL + FV + FL * FV * (RR - 1.f));

        // sheen
        Color3f Csheen = lerp(Color3f(1.f), Ctint, m_sheenTint);
        Color3f Fsheen = FH * m_sheen * Csheen;

        // specular
        Color3f F;
        Color3f specTerm = evaluateGGXSmithBRDF(V, L, Cspec0, roughness, m_anisotropy, F);         

        // clearcoat(ior = 1.5 -> F0 = 0.04)
        float clearcoatRoughness = math::lerp(m_clearcoatRoughness, .01f, .3f);
        Color3f clearcoatTerm = 0.25f * m_clearcoat * evaluateGGXSmithBRDF(V, L, 0.04f, clearcoatRoughness, m_anisotropy, F);       

        // auto st = shadowTerminator(L, bRec.its.shFrame.n, bRec.its.geoFrame.n);
        // auto st = shadowTerminator1(bRec.its.geoFrame.n, bRec.its.shFrame.n, L);

        return ((1.f-metallic)*(Cdlin * INV_PI * (Lambert + retro_reflection) +  Fsheen) +
                (specTerm + clearcoatTerm)) * Frame::cosTheta(bRec.wo);

    }

    float pdf(const BSDFQueryRecord &bRec) const override {
        /* This is a smooth BRDF -- return zero if the measure
           is wrong, or when queried for illumination on the backside */
        if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
            return 0.0f;

        // weight: reference - http://simon-kallweit.me/rendercompo2015/report/
        auto metallic = m_metallic->eval(bRec.uv).r();
        float diffuse = (1.f - metallic) * 0.5f;
        // float diffuse =  m_baseColor->eval(bRec.uv).maxCoeff();
        float GTR2 = 1.f / (1.f + m_clearcoat);

        auto H = (bRec.wi + bRec.wo).normalized();
        auto jacobian = 4.0f * bRec.wi.dot(H);
        
        // specular 
        // auto roughness = m_roughness->eval(bRec.uv).r();
        // if (bRec.its.regularizeFlag)
        //     roughness = std::min(1.f, roughness+bRec.its.accumulatedRoughness);
        auto roughness = std::min(1.f, m_roughness->eval(bRec.uv).r()+bRec.its.accumulatedRoughness);

        auto alpha = roughnessToAlpha(roughness, m_anisotropy);
        auto specPdf = computeGGXSmithPDF(bRec.wi, H, alpha) / jacobian;

        // clearcoat
        auto coatalpha = roughnessToAlpha(math::lerp(m_clearcoatRoughness, .01f, .3f), 0.f);
        float clearcoatPdf = computeGGXSmithPDF(bRec.wi, H, coatalpha) / jacobian;

        return diffuse * INV_PI * Frame::cosTheta(bRec.wo) +
                (1.f-diffuse) * (GTR2*specPdf + (1.f-GTR2)*clearcoatPdf);  
    }

    Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const override {
        if (Frame::cosTheta(bRec.wi) <= 0)
            return Color3f(0.0f);

        bRec.measure = ESolidAngle;
        /* Relative index of refraction: no change */
        bRec.eta = 1.0f;

        auto metallic = m_metallic->eval(bRec.uv).r();
        float diffuse = (1.f - metallic) * 0.5f;
        // float diffuse = m_baseColor->eval(bRec.uv).maxCoeff();

		if (sample1 < diffuse) {
            bRec.wo = Warp::squareToCosineHemisphere(sample2);
		} else {
			// auto sample = Point2f((_sample.x()-diffuse) / (1.f-diffuse), _sample.y());
            auto sample = (sample1-diffuse) / (1.f-diffuse);
            float GTR2 = 1.f / (1.f + m_clearcoat);
            
            Vector3f H;
            bool flip = bRec.wi.z() <= 0.f;
            if (sample < GTR2) {
                auto roughness = m_roughness->eval(bRec.uv).r();
                // if (bRec.its.regularizeFlag)
                //     roughness = std::min(1.f, roughness+bRec.its.accumulatedRoughness);
                // auto roughness = std::min(1.f, m_roughness->eval(bRec.uv).r()+bRec.its.accumulatedRoughness);
                Vector2f alpha = roughnessToAlpha(roughness, m_anisotropy);
                H = sampleGGXSmithVNDF(flip ? -bRec.wi : bRec.wi, alpha, sample2);
            } else {
                Vector2f alpha = roughnessToAlpha(math::lerp(m_clearcoatRoughness, 0.01f, .3f), 0.f);
                H = sampleGGXSmithVNDF(flip ? -bRec.wi : bRec.wi, alpha, sample2);
            }
            H = flip ? -H : H;
            // Reflect the view direction across the microfacet normal to get the sample direction.
            bRec.wo = reflect(bRec.wi, H).normalized();
		}
		// if (sample2.x() < diffuse) {
        //     auto sample = Point2f(sample2.x()/diffuse, sample2.y());
        //     bRec.wo = Warp::squareToCosineHemisphere(sample);
		// } else {
		// 	auto sample = Point2f((sample2.x()-diffuse) / (1.f-diffuse), sample2.y());
        //     float GTR2 = 1.f / (1.f + m_clearcoat);
            
        //     Vector3f H;
        //     Point2f sample1_;
        //     bool flip = bRec.wi.z() <= 0.f;
        //     if (sample1_.x() < GTR2) {
        //         sample1_ = Point2f(sample.x() / GTR2, sample.y());
        //         auto roughness = m_roughness->eval(bRec.uv).r();
        //         Vector2f alpha = roughnessToAlpha(roughness, m_anisotropy);
        //         H = sampleGGXSmithVNDF(flip ? -bRec.wi : bRec.wi, alpha, sample1_);
        //     } else {
        //         sample1_ = Point2f((sample.x()-GTR2) / (1.f-GTR2), sample.y());
        //         Vector2f alpha = roughnessToAlpha(math::lerp(m_clearcoatRoughness, 0.01f, .3f), 0.f);
        //         H = sampleGGXSmithVNDF(flip ? -bRec.wi : bRec.wi, alpha, sample1_);
        //     }
        //     H = flip ? -H : H;
        //     // Reflect the view direction across the microfacet normal to get the sample direction.
        //     bRec.wo = reflect(bRec.wi, H).normalized();
		// }

        auto invalid = [&]() {
            return std::isnan(bRec.wo.x()) || std::isnan(bRec.wo.y()) || std::isnan(bRec.wo.z());
        };

        if (Frame::cosTheta(bRec.wo) <= 0 || pdf(bRec) <= Epsilon || invalid())
            return Color3f(0.f);

		// return eval(bRec) / pdf(bRec) * Frame::cosTheta(bRec.wo);
        return eval(bRec) / pdf(bRec);
    }

    void addChild(Object *obj) override {
        switch (obj->getClassType()) {
            case ETexture:
                if( obj->getId() == "baseColor" ) {
                    if (m_baseColor)
                        throw Exception("There is already an baseColor defined!");
                    m_baseColor = static_cast<Texture<Color3f> *>(obj);
                }
                else if ( obj->getId() == "metallic" ) {
                    if (m_metallic)
                        throw Exception("There is already an metallic defined!");
                    m_metallic = static_cast<Texture<Color3f> *>(obj);
                }  
                else if ( obj->getId() == "roughness" ) {
                    if (m_roughness)
                        throw Exception("There is already an roughness defined!");
                    m_roughness = static_cast<Texture<Color3f> *>(obj);
                }       
                break;
            default:
                throw Exception("addChild is not supported other than baseColor maps");
        }
    }

    float regularize(const Point2f &uv) const {
        return m_roughness->eval(uv).r();
    }

    std::string toString() const {
        return fmt::format(
            "KazenStandardSurface"
        );
    }

private:
    Texture<Color3f>* m_baseColor=nullptr;
    Texture<Color3f>* m_roughness=nullptr;
    Texture<Color3f>* m_metallic=nullptr;
    float m_anisotropy;
    float m_specular;
    float m_specularTint;
    float m_sheen;
    float m_sheenTint;
    float m_clearcoat;
    float m_clearcoatRoughness;
};


// /// kazen principled bsdf
// class KazenPrincipledBSDF : public BSDF {
// public:
//     KazenPrincipledBSDF(const PropertyList &proplist) {
//         m_anisotropy = proplist.getFloat("anisotropy", 0.0f);
//         m_specular = proplist.getFloat("specular", 0.5f);
//         m_specularTint = proplist.getFloat("specularTint", 0.5f); 
//         m_clearcoat = proplist.getFloat("clearcoat", 0.0f);
//         m_clearcoatRoughness = proplist.getFloat("clearcoatRoughness", 0.5f);               
//         m_sheen = proplist.getFloat("sheen", 0.0f);
//         m_sheenTint = proplist.getFloat("sheenTint", 0.5);
//         m_transmission = proplist.getFloat("transmission", 0.0);
//     }

//     ~KazenPrincipledBSDF() {
//         if(!m_baseColor) delete m_baseColor;
//         if(!m_metallic) delete m_metallic;
//         if(!m_roughness) delete m_roughness;
//     }

//     // Color3f eval(const BSDFQueryRecord &bRec) const {
//     //     /* This is a smooth BRDF -- return zero if the measure
//     //        is wrong, or when queried for illumination on the backside */
//     //     if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
//     //         return Color3f(0.0f);

//     //     Vector3f V = bRec.wi;
//     //     Vector3f L = bRec.wo;
//     //     Vector3f H = (V + L).normalized();

//     //     // color 
//     //     Color3f Cdlin = m_baseColor->eval(bRec.uv);
//     //     auto metallic = m_metallic->eval(bRec.uv).r(); // TODO: single channel?
//     //     auto roughness = m_roughness->eval(bRec.uv).r();
//     //     float Cdlum = Cdlin.getLuminance();
//     //     Color3f Ctint = Cdlum > 0.f ? Cdlin/Cdlum : Color3f(1.f);
// 	// 	Color3f Ctintmix = 0.08f * m_specular * lerp(Color3f(1.f), Ctint, m_specularTint);
// 	// 	Color3f Cspec0 = lerp(Ctintmix, Cdlin, metallic);

//     //     // diffuse
//     //     float FL = schlickWeight(L.z());
//     //     float FV = schlickWeight(V.z());
//     //     float FH = schlickWeight(L.dot(H));

//     //     float cosThetaD = V.dot(H);
//     //     float FD90 = 0.5f * 2 * roughness * cosThetaD * cosThetaD;

//     //     float Lambert = (1.f - 0.5f*FL) * (1.f - 0.5f*FV);
//     //     float RR = 2.f * roughness * cosThetaD * cosThetaD;
//     //     float retro_reflection = RR * (FL + FV + FL * FV * (RR - 1.f));

//     //     // sheen
//     //     Color3f Csheen = lerp(Color3f(1.f), Ctint, m_sheenTint);
//     //     Color3f Fsheen = FH * m_sheen * Csheen;

//     //     // specular reflection
//     //     Color3f F;
//     //     Color3f specTerm = evaluateGGXSmithBRDF(V, L, Cspec0, roughness, m_anisotropy, F);         

//     //     // specular refraction


//     //     // clearcoat(ior = 1.5 -> F0 = 0.04)
//     //     float clearcoatRoughness = math::lerp(m_clearcoatRoughness, .01f, .3f);
//     //     Color3f clearcoatTerm = 0.25f * m_clearcoat * evaluateGGXSmithBRDF(V, L, 0.04f, clearcoatRoughness, m_anisotropy, F);       

//     //     return ((1.f-metallic)*(Cdlin * INV_PI * (Lambert + retro_reflection) +  Fsheen) +
//     //             (specTerm + clearcoatTerm)) * Frame::cosTheta(bRec.wo);

//     // }

//     // float pdf(const BSDFQueryRecord &bRec) const override {
//     //     /* This is a smooth BRDF -- return zero if the measure
//     //        is wrong, or when queried for illumination on the backside */
//     //     if (Frame::cosTheta(bRec.wi) <= 0 || Frame::cosTheta(bRec.wo) <= 0)
//     //         return 0.0f;

//     //     // weight: reference - http://simon-kallweit.me/rendercompo2015/report/
//     //     auto metallic = m_metallic->eval(bRec.uv).r();
//     //     float diffuse = (1.f - metallic) * 0.5f;
//     //     // float diffuse =  m_baseColor->eval(bRec.uv).maxCoeff();
//     //     float GTR2 = 1.f / (1.f + m_clearcoat);

//     //     auto H = (bRec.wi + bRec.wo).normalized();
//     //     auto jacobian = 4.0f * bRec.wi.dot(H);
        
//     //     // specular 
//     //     auto roughness = m_roughness->eval(bRec.uv).r();
//     //     auto alpha = roughnessToAlpha(roughness, m_anisotropy);
//     //     auto specPdf = computeGGXSmithPDF(bRec.wi, H, alpha) / jacobian;

//     //     // clearcoat
//     //     auto coatalpha = roughnessToAlpha(math::lerp(m_clearcoatRoughness, .01f, .3f), 0.f);
//     //     float clearcoatPdf = computeGGXSmithPDF(bRec.wi, H, coatalpha) / jacobian;

//     //     return diffuse * INV_PI * Frame::cosTheta(bRec.wo) +
//     //             (1.f-diffuse) * (GTR2*specPdf + (1.f-GTR2)*clearcoatPdf);  
//     // }

//     // Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const override {
//     //     if (Frame::cosTheta(bRec.wi) <= 0)
//     //         return Color3f(0.0f);

//     //     bRec.measure = ESolidAngle;
//     //     /* Relative index of refraction: no change */
//     //     bRec.eta = 1.0f;

//     //     auto metallic = m_metallic->eval(bRec.uv).r();
//     //     float diffuse = (1.f - metallic) * 0.5f;
//     //     // float diffuse = m_baseColor->eval(bRec.uv).maxCoeff();

// 	// 	if (sample1 < diffuse) {
//     //         bRec.wo = Warp::squareToCosineHemisphere(sample2);
// 	// 	} else {
// 	// 		// auto sample = Point2f((_sample.x()-diffuse) / (1.f-diffuse), _sample.y());
//     //         auto sample = (sample1-diffuse) / (1.f-diffuse);
//     //         float GTR2 = 1.f / (1.f + m_clearcoat);
            
//     //         Vector3f H;
//     //         bool flip = bRec.wi.z() <= 0.f;
//     //         if (sample < GTR2) {
//     //             auto roughness = m_roughness->eval(bRec.uv).r();
//     //             Vector2f alpha = roughnessToAlpha(roughness, m_anisotropy);
//     //             H = sampleGGXSmithVNDF(flip ? -bRec.wi : bRec.wi, alpha, sample2);
//     //         } else {
//     //             Vector2f alpha = roughnessToAlpha(math::lerp(m_clearcoatRoughness, 0.01f, .3f), 0.f);
//     //             H = sampleGGXSmithVNDF(flip ? -bRec.wi : bRec.wi, alpha, sample2);
//     //         }
//     //         H = flip ? -H : H;
//     //         // Reflect the view direction across the microfacet normal to get the sample direction.
//     //         bRec.wo = reflect(bRec.wi, H).normalized();
// 	// 	}

//     //     auto invalid = [&]() {
//     //         return std::isnan(bRec.wo.x()) || std::isnan(bRec.wo.y()) || std::isnan(bRec.wo.z());
//     //     };

//     //     if (Frame::cosTheta(bRec.wo) <= 0 || pdf(bRec) <= Epsilon || invalid())
//     //         return Color3f(0.f);

// 	// 	// return eval(bRec) / pdf(bRec) * Frame::cosTheta(bRec.wo);
//     //     return eval(bRec) / pdf(bRec);
//     // }



//     Color3f eval(const BSDFQueryRecord &bRec) const {
//         // if (Frame::cosTheta(bRec.wi) <= 0.f || Frame::cosTheta(bRec.wo) <= 0.f)
//         //     return Color3f(0.f);
//         Vector3f V = bRec.wi;
//         Vector3f L = bRec.wo;
//         Vector3f H = (V + L).normalized();

//         // material parameters
//         auto metallic = m_metallic->eval(bRec.uv).r(); // TODO: single channel?
//         auto roughness = m_roughness->eval(bRec.uv).r();
        
//         // color 
//         Color3f Cdlin = m_baseColor->eval(bRec.uv);
//         float Cdlum = Cdlin.getLuminance();
//         Color3f Ctint = Cdlum > 0.f ? Cdlin/Cdlum : Color3f(1.f);
// 		// Color3f Ctintmix = 0.08f * m_specular * lerp(Color3f(1.f), Ctint, m_specularTint);
// 		// Color3f Cspec0 = lerp(Ctintmix, Cdlin, metallic);  
    
//         // diffuse
//         float diffuseWt = (1.f - metallic) * (1.f - m_transmission);
//         Color3f diffuse = diffuseWt > 0.f ? 
//             diffuseWt * evalDiffuse(V, L, H, Cdlin, Ctint, roughness) : Color3f(0.f);


//         // specular transmission
//         float transmissionWt = (1.f - metallic) * m_transmission;
//         Color3f transmission = transmissionWt > 0.f ? 
//             transmissionWt * evalTransmission(V, L, H, Cdlin, Ctint, roughness, metallic) : Color3f(0.f);

//         // specular reflection
//         Color3f reflection = evalReflection(V, L, H, Cdlin, Ctint, roughness, metallic);

//         // clearcoat
//         Color clearcoat = evalClearcoat();

//         return (diffuse + reflection + transmission + clearcoat) * Frame::cosTheta(bRec.wo);
//     }


//     float pdf(const BSDFQueryRecord &bRec) const {
//         Vector3f V = bRec.wi;
//         Vector3f L = bRec.wo;
//         Vector3f H = (V + L).normalized();

//         float lum = m_baseColor->eval(bRec.uv).getLuminance();
//         auto metallic = m_metallic->eval(bRec.uv).r();
//         auto roughness = m_roughness->eval(bRec.uv).r();
        
//         float diffuseWt = (1.f - metallic) * (1.f - m_transmission) * 0.5f;
//         float transmissionWt = (1.f - metallic) * m_transmission * 0.5f;
//         float reflectionWt = (1.f - diffuseWt - transmissionWt) / (1.f + m_clearcoat);
//         float clearcoatWt = 1.f - diffuseWt - transmissionWt - reflectionWt;

//         auto diffusePdf = diffuseWt * pdfDiffuse(V, L);
//         auto reflectionPdf = reflectionWt * pdfReflection(V, L, H, roughness);
//         auto transmissionPdf = transmissionWt * pdfTransmission(V, L, H, roughness, metallic);
//         auto clearcoatPdf = clearcoatWt * pdfClearcoat(V, L, H, roughness);
        
//         return diffusePdf + reflectionPdf + transmissionPdf + clearcoatPdf;
//     }


//     Color3f sample(BSDFQueryRecord &bRec, float sample1, const Point2f &sample2) const override {
//         if (Frame::cosTheta(bRec.wi) <= 0.f)
//             return Color3f(0.f);

//         bRec.measure = ESolidAngle;
        
//         /* Relative index of refraction: no change */
//         bRec.eta = 1.0f;

//         auto metallic = m_metallic->eval(bRec.uv).r();
//         auto roughness = m_roughness->eval(bRec.uv).r();

//         float diffuseWt = (1.f - metallic) * (1.f - m_transmission) * 0.5f;
//         float transmissionWt = (1.f - metallic) * m_transmission * 0.5f;
//         float reflectionWt = (1.f - diffuseWt - transmissionWt) / (1.f + m_clearcoat);
//         float clearcoatWt = 1.f - diffuseWt - transmissionWt - reflectionWt;

// 		if (sample1 < diffuseWt) {
//             bRec.wo = Warp::squareToCosineHemisphere(sample2);
//         }
//         else if (sample1 < diffuseWt + clearcoatWt) {
//             bRec.wo = Warp::squareToCosineHemisphere(sample2);
//         }
//         else {
//             // bRec.wo = sampleClearcoat(V, L, H, roughness, sample2);
//         } 


// 		if (sample1 < diffuseWt) {
//             bRec.wo = Warp::squareToCosineHemisphere(sample2);
// 		} else {
//             auto alpha = MicrofacetDistribution::roughnessToAlpha(roughness, m_anisotropy);
//             MicrofacetDistribution distr(alpha[0], alpha[1]);

//             // bool flip = bRec.wi.z() <= 0.f;
//             auto H = distr.sample(bRec.wi, sample2);
//             if (H.z() <= 0.f)
//                 H = -H;
//             // H = flip ? -H : H;

//             bRec.wo = reflect(bRec.wi, H).normalized();
//         }

//         auto invalid = [&]() { return std::isnan(bRec.wo.x()) || std::isnan(bRec.wo.y()) || std::isnan(bRec.wo.z()); };
//         if (pdf(bRec) <= Epsilon || invalid())
//             return Color3f(0.f);

//         return eval(bRec) / pdf(bRec);
//     }


//     void addChild(Object *obj) override {
//         switch (obj->getClassType()) {
//             case ETexture:
//                 if( obj->getId() == "baseColor" ) {
//                     if (m_baseColor)
//                         throw Exception("There is already an baseColor defined!");
//                     m_baseColor = static_cast<Texture<Color3f> *>(obj);
//                 }
//                 else if ( obj->getId() == "metallic" ) {
//                     if (m_metallic)
//                         throw Exception("There is already an metallic defined!");
//                     m_metallic = static_cast<Texture<Color3f> *>(obj);
//                 }  
//                 else if ( obj->getId() == "roughness" ) {
//                     if (m_roughness)
//                         throw Exception("There is already an roughness defined!");
//                     m_roughness = static_cast<Texture<Color3f> *>(obj);
//                 }       
//                 break;
//             default:
//                 throw Exception("addChild is not supported other than baseColor maps");
//         }
//     }

//     std::string toString() const {
//         return fmt::format(
//             "KazenPrincipledBSDF"
//         );
//     }

// private:
//     float schlickWeight(float x) const {
//         x = math::clamp(1.f - x, 0.f, 1.f);
//         auto x2 = x * x;
//         return x2 * x2 * x;
//     }

//     Color3f lerp(const Color3f& c1, const Color3f& c2, float t) const {
//         return (1.f - t) * c1 + t * c2;
//     }

//     float ior(float specular) const {
//         return 2.f/(1.f - std::sqrt(0.08*specular)) - 1.f;
//     };

//     float schlickR0FromEta(float eta) const { return sqr(eta - 1) / sqr(eta + 1); }

//     Color3f frSchlick(Color3f R0, float cosTheta) const {
//         return lerp(R0, Color3f(1.f), schlickWeight(cosTheta));
//     }

//     float frDielectric(float cosThetaI, float eta) const {
//         cosThetaI = math::clamp(cosThetaI, -1.f, 1.f);
//         // Potentially flip interface orientation for Fresnel equations
//         if (cosThetaI < 0) {
//             eta = 1 / eta;
//             cosThetaI = -cosThetaI;
//         }

//         // Compute cosThetaT for Fresnel equations using Snell's law
//         float sin2ThetaI = 1 - sqr(cosThetaI);
//         float sin2ThetaT = sin2ThetaI / sqr(eta);
//         if (sin2ThetaT >= 1)
//             return 1.f;
//         float cosThetaT = std::sqrt(1 - sin2ThetaT);

//         float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
//         float Rp = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
        
//         return 0.5f * (sqr(Rs) + sqr(Rp));
//         // return 0.f;
//     }


//     Color3f disneyFresnel(Vector3f V, Vector3f H, Color3f Cspec, float eta, float metallic) const {
//         float cosThetaV = V.dot(H);

//         Color3f metallicFresnel = frSchlick(Cspec, cosThetaV);
//         Color3f dielectricFresnel = Color3f(frDielectric(cosThetaV, eta));
//         return lerp(dielectricFresnel, metallicFresnel, metallic);
//         // return 0.f;
//     }

//     Color3f evalDiffuse(Vector3f V, Vector3f L, Vector3f H, Color3f Cdlin, Color3f Ctint, float roughness) const {
//         if (Frame::cosTheta(V) <= 0.f || Frame::cosTheta(L) <= 0.f)
//             return Color3f(0.f);
        
//         float FL = schlickWeight(L.z());
//         float FV = schlickWeight(V.z());
//         float FH = schlickWeight(L.dot(H));

//         // diffuse
//         float diffuse = (1.f - 0.5f*FL) * (1.f - 0.5f*FV);
        
//         // retro-reflection
//         float cosThetaD = V.dot(H);
//         float RR = 2.f * roughness * cosThetaD * cosThetaD;
//         float retro_reflection = RR * (FL + FV + FL * FV * (RR - 1.f));

//         // sheen
//         Color3f Csheen = lerp(Color3f(1.f), Ctint, m_sheenTint);
//         Color3f Fsheen = FH * m_sheen * Csheen;

//         return Cdlin * INV_PI * (diffuse + retro_reflection) + Fsheen; 
//     }

//     Color3f evalReflection(Vector3f V, Vector3f L, Vector3f H, Color3f Cdlin, Color3f Ctint, float roughness, float metallic) const {
//         if (Frame::cosTheta(V) <= 0.f || Frame::cosTheta(L) <= 0.f)
//             return Color3f(0.f);

//         auto eta = ior(m_specular);
//         auto R0 = schlickR0FromEta(eta);

//         Color3f Cspec0 = lerp(R0*lerp(Color3f(1.f), Ctint, m_specularTint), Cdlin, metallic);
//         Color3f F = disneyFresnel(V, H, Cspec0, eta, metallic);

//         auto alpha = MicrofacetDistribution::roughnessToAlpha(roughness, m_anisotropy);
//         MicrofacetDistribution distr(alpha[0], alpha[1]);

//         // Compute the D (NDF) and G (visibility) terms, along with the microfacet BRDF
//         // denominator.
//         float D = distr.D(H);
//         float G = distr.G2(V, L);
//         float denom = 4.0f * abs(V.z()) * abs(L.z());

//         // Return the combined microfacet expression.
//         return F * D * G / denom;
//         // return Color3f(1.f);
//     }

//     Color3f evalTransmission(Vector3f V, Vector3f L, Vector3f H, float eta_ ) const {
//         // if (Frame::cosTheta(L) > 0.f)
//         //     return Color3f(0.f);

//         // float eta =  Frame::cosTheta(V) > 0.f ? eta_ : 1.f/eta_;

//         // float F = frDielectric(V.dot(H), eta);
        
//         // auto alpha = MicrofacetDistribution::roughnessToAlpha(roughness, m_anisotropy)
//         // MicrofacetDistribution distr(alpha[0], alpha[1]);

//         // // Compute the D (NDF) and G (visibility) terms, along with the microfacet BRDF
//         // // denominator.
//         // float D =  distr.D(H);
//         // float G = distr.G2(V, L);
//         // // float denom = 4.0f * abs(V.z()) * abs(L.z());

//         // float sqrtDenom = bRec.wi.dot(wm) + eta * bRec.wo.dot(wm);
//         //     float value = ((1 - F) * D * G * eta * eta * bRec.wi.dot(wm) * bRec.wo.dot(wm)) /
//         //         (cosThetaI * sqr(denom));

//         //     return std::abs(value);
//         return Color3f(0.f);
//     }

//     Color3f evalClearcoat() const {
//         return Color3f(0.f);
//     }

//     float pdfDiffuse(Vector3f V, Vector3f L) const {
//         if (Frame::cosTheta(V) <= 0.f || Frame::cosTheta(L) <= 0.f)
//             return 0.f;

//         return INV_PI * Frame::cosTheta(L);
//     }

//     float pdfReflection(Vector3f V, Vector3f L, Vector3f H, float roughness) const {
//         if (Frame::cosTheta(V) <= 0.f || Frame::cosTheta(L) <= 0.f)
//             return 0.f;

//         auto jacobian = 4.0f * V.dot(H);

//         auto alpha = MicrofacetDistribution::roughnessToAlpha(roughness, m_anisotropy);
//         MicrofacetDistribution distr(alpha[0], alpha[1]);

//         return distr.pdf(V, H) / jacobian;
//     }

//     float pdfTransmission(ector3f V, Vector3f L, Vector3f H, float roughness, float metallic) const {
//         return 0.f;
//     }

//     float pdfClearcoat(Vector3f V, Vector3f L, Vector3f H, float roughness) const {
//         return 0.f;
//     }


// private:
//     Texture<Color3f>* m_baseColor=nullptr;
//     Texture<Color3f>* m_roughness=nullptr;
//     Texture<Color3f>* m_metallic=nullptr;
//     float m_anisotropy;
//     float m_specular;
//     float m_specularTint;
//     float m_sheen;
//     float m_sheenTint;
//     float m_clearcoat;
//     float m_clearcoatRoughness;
//     float m_transmission;
// };



KAZEN_REGISTER_CLASS(Diffuse, "diffuse");
KAZEN_REGISTER_CLASS(Dielectric, "dielectric");
KAZEN_REGISTER_CLASS(Mirror, "mirror");
KAZEN_REGISTER_CLASS(Lambertian, "lambertian");
KAZEN_REGISTER_CLASS(NormalMap, "normalmap");
// KAZEN_REGISTER_CLASS(NormalMapMicrofacet, "normalmap_mircofacet");
KAZEN_REGISTER_CLASS(GGX, "ggx");
KAZEN_REGISTER_CLASS(RoughConductor, "roughconductor");
KAZEN_REGISTER_CLASS(RoughPlastic, "roughplastic");
KAZEN_REGISTER_CLASS(RoughDielectric, "roughdielectric");
KAZEN_REGISTER_CLASS(KazenStandardSurface, "kazenstandard");
// KAZEN_REGISTER_CLASS(KazenPrincipledBSDF, "principled");
NAMESPACE_END(kazen)
