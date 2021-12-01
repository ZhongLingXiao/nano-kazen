#include <kazen/integrator.h>
#include <kazen/scene.h>

NAMESPACE_BEGIN(kazen)

class NormalIntegrator : public Integrator {
public:
    NormalIntegrator(const PropertyList &props) {
        /* No parameters this time */
        // LOG("intergrator: normal");
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Color3f(0.0f);

        /* Return the component-wise absolute
           value of the shading normal as a color */
        Normal3f n = its.shFrame.n.cwiseAbs();
        return Color3f(n.x(), n.y(), n.z());
    }

    std::string toString() const {
        return "NormalIntegrator[]";
    }
};


class SimpleIntegrator : public Integrator {
public:
    SimpleIntegrator(const PropertyList &props) {
        /* No parameters this time */
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {

        return Color3f(0.0f, 0.0f, 0.0f);
    }

    std::string toString() const {
        return "SimpleIntegrator[]";
    }
};

KAZEN_REGISTER_CLASS(NormalIntegrator, "normals");
KAZEN_REGISTER_CLASS(SimpleIntegrator, "simple");
NAMESPACE_END(kazen)