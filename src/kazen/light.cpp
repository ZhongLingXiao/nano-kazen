#include <kazen/light.h>
#include <kazen/warp.h>
#include <kazen/mesh.h>

NAMESPACE_BEGIN(kazen)

class AreaLight : public Light {
public:
    AreaLight(const PropertyList &propList) {
        m_radiance = propList.getColor("radiance");
    }

    Color3f eval(const LightQueryRecord &lRec) const override {
        auto cosTheta = lRec.n.dot(-lRec.wi);
        return cosTheta > 0.f ?  m_radiance : 0.f;
    }

    Color3f sample(const Mesh *mesh, LightQueryRecord &lRec, Sampler *sampler) const override {       
        mesh->sample(sampler, lRec.p, lRec.n);
        lRec.wi = (lRec.p - lRec.ref).normalized();
        lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, (lRec.p-lRec.ref).norm()-Epsilon);
        
        /* Calculate geometric term: G(x<->y) = |ny*(y->x)| / ||x-y||^2 
         * |nx*(x->y)| using its data, so it should calculate in integrator
        */
        lRec.pdf = pdf(mesh, lRec);
        if (lRec.pdf > 0.f && !std::isnan(lRec.pdf) && !std::isinf(lRec.pdf)) {
            return eval(lRec) / lRec.pdf; // divided by the probability of the sample y per unit area.
        }
        return Color3f(0.f);
    }

    float pdf(const Mesh* mesh, const LightQueryRecord &lRec) const override {
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
};

KAZEN_REGISTER_CLASS(AreaLight, "area")
NAMESPACE_END(kazen)