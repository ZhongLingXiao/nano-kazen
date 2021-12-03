#include <kazen/integrator.h>
#include <kazen/scene.h>
#include <kazen/warp.h>
#include <kazen/light.h>
#include <kazen/bsdf.h>

NAMESPACE_BEGIN(kazen)

/// normals
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
            return Color3f(0.f);

        /* Return the component-wise absolute
           value of the shading normal as a color */
        Normal3f n = its.shFrame.n.cwiseAbs();
        return Color3f(n.x(), n.y(), n.z());
    }

    std::string toString() const {
        return "NormalIntegrator[]";
    }
};


/// simple
class SimpleIntegrator : public Integrator {
public:
    SimpleIntegrator(const PropertyList &props) {
        m_position = props.getPoint("position");
        m_energy = props.getColor("energy");
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if (!scene->rayIntersect(ray, its)) {
            return Color3f(0.f);
        }

        auto hitPos = its.p;
        Vector3f wo = m_position - hitPos;
        auto distance = wo.dot(wo);
        wo.normalize();
        
        auto albedo = m_energy / (4 * M_PI * M_PI);
        Ray3f shadowRay = Ray3f(hitPos, wo);
        auto cosTheta = std::max(0.f, its.shFrame.cosTheta(its.shFrame.toLocal(wo)));
        
        if(!scene->rayIntersect(shadowRay)) {
            return albedo * cosTheta / distance;
        }
        
        return Color3f(0.f);
    }

    std::string toString() const {
        return "SimpleIntegrator[]";
    }

private:
    Point3f m_position; 
    Color3f m_energy;  
};


/// ao
class AmbientOcclusionIntegrator : public Integrator {
public:
    AmbientOcclusionIntegrator(const PropertyList &props) {

    }
    
    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if(!scene->rayIntersect(ray, its)) {
            return Color3f(0.f);
        }

        auto sample = Warp::squareToUniformHemisphere(sampler->next2D());
        auto point = its.toWorld(sample);
        auto shadowRay = Ray3f(its.p, point);
        
        if(!scene->rayIntersect(shadowRay)) {
            its.shFrame.n.normalize();
            point.normalize();
            auto cosTheta = its.shFrame.cosTheta(its.shFrame.toLocal(point));
            return Color3f((cosTheta / M_PI)) / Warp::squareToUniformHemispherePdf(sample);
        }
        
        return Color3f(0.f);
    }
    
    std::string toString() const {
        return "AmbientOcculusionIntegrator[]";
    }
    
private:

};


/// Whitted
class WhittedIntegrator : public Integrator {
public:
    WhittedIntegrator(const PropertyList& props) {}
    
    Color3f Li(const Scene* scene, Sampler* sampler, const Ray3f& ray) const {        
        Intersection its;

        if (!scene->rayIntersect(ray, its)) {
            return Color3f(0.f);
        }
    
        Color3f Le(0.0f);
        if (its.mesh->isLight()) {
            LightQueryRecord rec(ray.o, its.p, its.shFrame.n);
            Le = its.mesh->getLight()->eval(rec);
        }

        auto light = scene->getRandomLight();
        LightQueryRecord rec(its.p);
        Color3f li = light->getLight()->sample(light, rec, sampler);
        if (scene->rayIntersect(rec.shadowRay)) {
            li = 0;
        }

        float cosTheta = Frame::cosTheta(its.shFrame.toLocal(rec.wi));
        BSDFQueryRecord bRec(its.toLocal(-ray.d), its.toLocal(rec.wi), ESolidAngle);
        Color3f f = its.mesh->getBSDF()->eval(bRec);
        if (cosTheta < 0) {
            cosTheta = 0;
        }

        return Le + li * f * cosTheta / scene->getLightPdf();
    }
    
    std::string toString() const {
        return "WhittedIntegrator[]";
    }
};


KAZEN_REGISTER_CLASS(NormalIntegrator, "normals");
KAZEN_REGISTER_CLASS(SimpleIntegrator, "simple");
KAZEN_REGISTER_CLASS(AmbientOcclusionIntegrator, "ao");
KAZEN_REGISTER_CLASS(WhittedIntegrator, "whitted");
NAMESPACE_END(kazen)