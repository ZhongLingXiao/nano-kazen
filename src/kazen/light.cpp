#include <kazen/light.h>
#include <kazen/warp.h>
#include <kazen/mesh.h>

NAMESPACE_BEGIN(kazen)

class AreaLight : public Light {
public:
    AreaLight(const PropertyList &propList) {
        m_radiance = propList.getColor("radiance");
    }

    Color3f eval(const LightQueryRecord &rec_) const override {
        const LightQueryRecord& rec = rec_;
        return (rec.n.dot(rec.wi) < 0.0f) ? m_radiance : 0.0f;
    }

    Color3f sample(Mesh *mesh, LightQueryRecord &rec, Sampler *sampler) const override {
        mesh->sample(sampler, rec.p, rec.n, rec.pdf);
        rec.wi = (rec.p - rec.ref).normalized();
        rec.shadowRay = Ray3f(rec.ref, rec.wi, Epsilon, (rec.p-rec.ref).norm()-Epsilon);
        rec.pdf = pdf(mesh, rec);
        if (rec.pdf > 0.0f) {
            return eval(rec) / rec.pdf;
        }
        return Color3f(0.0f);
    }

    float pdf(Mesh* mesh, const LightQueryRecord &rec) const override {
        float cosTheta = rec.n.dot(-rec.wi);
        if (cosTheta > 0.0f) {
            auto distance2 = (rec.p - rec.ref).squaredNorm();
            return rec.pdf * distance2 / cosTheta;
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