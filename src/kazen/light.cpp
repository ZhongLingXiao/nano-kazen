#include <kazen/light.h>
#include <kazen/warp.h>
#include <kazen/mesh.h>

NAMESPACE_BEGIN(kazen)

class AreaLight : public Light {
public:
    AreaLight(const PropertyList &propList) {
        m_color = propList.getColor("color", Color3f(1.f));
        m_intensity = propList.getFloat("intensity", 1.f);
        m_radiance = m_intensity*m_color;
    }

    Color3f eval(const LightQueryRecord &lRec) const override {
        auto cosTheta = lRec.n.dot(-lRec.wi);
        return cosTheta > 0.f ?  m_radiance : 0.f;
    }

    Color3f sample(LightQueryRecord &lRec, Sampler *sampler, const Mesh *mesh) const override {       
        mesh->sample(sampler, lRec.p, lRec.n);
        lRec.wi = (lRec.p - lRec.ref).normalized();
        lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, (lRec.p-lRec.ref).norm()-Epsilon);
        
        /* Calculate geometric term: G(x<->y) = |ny*(y->x)| / ||x-y||^2 
         * |nx*(x->y)| using its data, so it should calculate in integrator
        */
        lRec.pdf = pdf(lRec, mesh);
        if (lRec.pdf > 0.f && !std::isnan(lRec.pdf) && !std::isinf(lRec.pdf)) {
            return eval(lRec) / lRec.pdf; // divided by the probability of the sample y per unit area.
        }
        return Color3f(0.f);
    }

    float pdf(const LightQueryRecord &lRec, const Mesh *mesh) const override {
        float pdf = mesh->pdf(); // mesh pdf
        float cosTheta = lRec.n.dot(-lRec.wi);
        /* For balance heuristic: Transform the integration variable from the position domain to solid angle domain
         *
         * Remember that this only makes sense if both probabilities are expressed in the same measure 
         * (i.e. with respect to solid angles or unit area). This means that you will have convert one 
         * of them to the measure of the other (which one doesn't matter).
         * pbrt-v3: 14.2.2 Sampling Shapes
        */
        if (cosTheta > 0.f) {
            auto distance2 = (lRec.p - lRec.ref).squaredNorm();
            return pdf * distance2 / cosTheta;
        }
        return 0.f; // if back-facing surface encountered
    }


    std::string toString() const override {
        return "AreaLight[]";
    }

private:
    Color3f m_radiance;
    Color3f m_color;
    float m_intensity;
};


/// point light 
class PointLight : public Light {
public:
	PointLight(const PropertyList &propList) {
		m_position = propList.getPoint("position", Point3f(0.f));
		m_power = propList.getColor("power", Color3f(1.f));
	}

    Color3f sample(LightQueryRecord &lRec, Sampler *sampler, const Mesh *mesh) const override {
		lRec.wi = (m_position - lRec.ref).normalized();
		lRec.p = m_position;
		lRec.pdf = 1.f;
		lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, (m_position - lRec.ref).norm() - Epsilon);
		return m_power / (4.f * M_PI * (m_position - lRec.ref).squaredNorm());
	}

	Color3f eval(const LightQueryRecord &lRec) const override {
		return m_power / (4.f * M_PI * (m_position - lRec.ref).squaredNorm());
	}

    float pdf(const LightQueryRecord &lRec, const Mesh *mesh) const override {
		return 1.f;
	}

	virtual std::string toString() const {
		return "PointLight[]";
	}

private:
	Color3f m_power;
	Point3f m_position;
};


KAZEN_REGISTER_CLASS(AreaLight, "area")
KAZEN_REGISTER_CLASS(PointLight, "point")
NAMESPACE_END(kazen)