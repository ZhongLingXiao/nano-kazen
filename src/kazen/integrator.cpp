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
    
        /* Le term here to account for light sources that were direcly hit by the camera ray. */
        Color3f Le(0.f);
        if (its.mesh->isLight()) {
            LightQueryRecord leRec(ray.o, its.p, its.shFrame.n);
            Le = its.mesh->getLight()->eval(leRec);
        }

        if (its.mesh->getBSDF()->isDiffuse()) {
            /* Select a random light mesh with mis, it should divide its pdf */
            auto light = scene->getRandomLight(sampler->next1D());

            /* Get light sample: Ls(y, y->x) */
            LightQueryRecord rec(its.p);
            Color3f Ls = light->getLight()->sample(light, rec, sampler);
            if (scene->rayIntersect(rec.shadowRay)) {
                Ls = 0.f;
            }

            /* cosine factor is |nx * (x->y)| */
            float cosTheta = Frame::cosTheta(its.shFrame.toLocal(rec.wi));
            if (cosTheta < 0.f) {
                cosTheta = 0.f;
            }

            /* Calculate bsdf factor */
            BSDFQueryRecord bRec(its.toLocal(-ray.d), its.toLocal(rec.wi), ESolidAngle);
            Color3f f = its.mesh->getBSDF()->eval(bRec);

            /* reflection equation */
            auto Lr = f * Ls * cosTheta;

            /* Final incident radiance at the camera */
            auto Li = Le + Lr/scene->getLightPdf();

            return Li;
        } else {
            BSDFQueryRecord bRec(its.toLocal(-ray.d));
            Color3f reflect = its.mesh->getBSDF()->sample(bRec, sampler->next2D());
            if (sampler->next1D() < 0.95) {
                return reflect * Li(scene, sampler, Ray3f(its.p, its.toWorld(bRec.wo))) / 0.95;
            } else {
                return Color3f(0.f);
            }
        }
    }
    
    std::string toString() const {
        return "WhittedIntegrator[]";
    }
};


// Next Event Estimation(NEE) with multiple importance sampling(MIS)
class PathMisIntegrator : public Integrator {
public:
    PathMisIntegrator(const PropertyList &propList) {}

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray_) const {
        Ray3f ray = ray_;
        Color3f Li(0.f), throughput(1.f);
        
        /* Tracks radiance scaling due to index of refraction changes */
        float eta = 1.f;

        /* MIS weight for intersected lights (set by prev. iteration) */
        float bsdfWeight(1.f);

        /* First intersection */
        Intersection its;
        if (!scene->rayIntersect(ray, its)) {
            return Li;
        }
    
        /* Tracks depth for Russian roulette */
        int depth = 1;
        while (true) {
            /* ----------------------- Intersection with lights ----------------------- */
            if (its.mesh->isLight()) {
                LightQueryRecord lRec(ray.o, its.p, its.shFrame.n);
                lRec.uv = its.uv;
                Li += bsdfWeight * throughput * its.mesh->getLight()->eval(lRec);
            }
      
            /* Russian roulette: try to keep path weights equal to one,
               while accounting for the solid angle compression at refractive
               index boundaries. Stop with at least some probability to avoid
               getting stuck (e.g. due to total internal reflection) */
            if (depth >= 3) {
                // continuation probability
                auto probability = std::min(throughput.maxCoeff()*eta*eta, 0.99f);
                if (probability < sampler->next1D()) {
                    break;
                }
                throughput /= probability;
            }

            /* ----------------------- Light sampling ----------------------- */
            const Mesh* mesh = scene->getRandomLight(sampler->next1D());
            const Light* light = mesh->getLight();
            LightQueryRecord lRec(its.p);
            lRec.uv = its.uv;
            Color3f Ls = light->sample(mesh, lRec, sampler) / scene->getLightPdf();
           
            auto lightPdf = light->pdf(mesh, lRec);
            if (!scene->rayIntersect(lRec.shadowRay)) {
                auto cosTheta = std::max(0.f, Frame::cosTheta(its.shFrame.toLocal(lRec.wi)));

                /* Query the BSDF for that emitter-sampled direction */
                BSDFQueryRecord bRec(its.toLocal(-ray.d), its.toLocal(lRec.wi), ESolidAngle);
                bRec.uv = its.uv;
                Color3f f = its.mesh->getBSDF()->eval(bRec);

                /* Determine density of sampling that same direction using BSDF sampling */
                auto bsdfPdf = its.mesh->getBSDF()->pdf(bRec);

                auto lightWeight = powerHeuristic(lightPdf, bsdfPdf);
                Li += throughput * Ls * f * cosTheta * lightWeight;
            }

            /* ----------------------- BSDF sampling ----------------------- */
            BSDFQueryRecord bRec(its.shFrame.toLocal(-ray.d));
            bRec.uv = its.uv;
            auto bsdfColor = its.mesh->getBSDF()->sample(bRec, sampler->next2D()); // Sample BSDF * cos(theta)
            throughput *= bsdfColor;
            eta *= bRec.eta;

            /*  Intersect the BSDF ray against the scene geometry */
            ray = Ray3f(its.p, its.toWorld(bRec.wo));
            auto bsdfPdf = its.mesh->getBSDF()->pdf(bRec);
            if (!scene->rayIntersect(ray, its)) {
                break;
            }

            /* Determine probability of having sampled that same
               direction using emitter sampling. */
            if (its.mesh->isLight()) {
                LightQueryRecord lRec_ = LightQueryRecord(ray.o, its.p, its.shFrame.n);
                lRec_.uv = its.uv;
                float lightPdf_ = its.mesh->getLight()->pdf(its.mesh, lRec_);
                bsdfWeight = powerHeuristic(bsdfPdf, lightPdf_);
            }

            if (bRec.measure == EDiscrete) {
                bsdfWeight = 1.f;
            }
            
            /* Increase depth for rr */
            depth++;
        }

        return Li;
    }

    inline float powerHeuristic(float pdfA, float pdfB) const {
        pdfA *= pdfA;
        pdfB *= pdfB;
        return pdfA > 0.f ? pdfA/(pdfA + pdfB): 0.f;
    }

  std::string toString() const {
      return "PathMisIntegrator[]";
  }
};


KAZEN_REGISTER_CLASS(NormalIntegrator, "normals");
KAZEN_REGISTER_CLASS(SimpleIntegrator, "simple");
KAZEN_REGISTER_CLASS(AmbientOcclusionIntegrator, "ao");
KAZEN_REGISTER_CLASS(WhittedIntegrator, "whitted");
KAZEN_REGISTER_CLASS(PathMisIntegrator, "path_mis");
NAMESPACE_END(kazen)