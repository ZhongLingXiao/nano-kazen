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
        auto cosTheta = lRec.n.dot(lRec.wi);
        return cosTheta< 0.f ?  m_radiance : 0.f;
    }

    Color3f sample(const Mesh *mesh, LightQueryRecord &lRec, Sampler *sampler) const override {       
        mesh->sample(sampler, lRec.p, lRec.n, lRec.pdf);
        lRec.wi = (lRec.p - lRec.ref).normalized();
        lRec.shadowRay = Ray3f(lRec.ref, lRec.wi, Epsilon, (lRec.p-lRec.ref).norm()-Epsilon);
        
        /* Calculate geometric term: G(x<->y) = |ny*(y->x)| / ||x-y||^2 
         * |nx*(x->y)| using its data, so it should calculate in integrator
        */
        lRec.pdf = pdf(mesh, lRec);
        if (lRec.pdf > 0.f) {
            auto cosTheta = lRec.n.dot(-lRec.wi);
            auto distance2 = (lRec.p - lRec.ref).squaredNorm();
            auto g = cosTheta / distance2;
            return g * eval(lRec) / lRec.pdf; // divided by the probability of the sample y per unit area.
        }
        return Color3f(0.0f);
    }

    float pdf(const Mesh* mesh, const LightQueryRecord &lRec) const override {
        float cosTheta = lRec.n.dot(-lRec.wi);
        if (cosTheta > 0.0f) {
            return lRec.pdf;
        }
        return 0.0f;
    }

    Color3f getRadiance() const override {
        return m_radiance;
    }

    std::string toString() const override {
        return "AreaLight[]";
    }

private:
    Color3f m_radiance;
};

KAZEN_REGISTER_CLASS(AreaLight, "area")
NAMESPACE_END(kazen)