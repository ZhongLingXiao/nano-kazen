#include <kazen/integrator.h>
#include <kazen/scene.h>
#include <kazen/warp.h>
#include <kazen/light.h>
#include <kazen/bsdf.h>
#include <kazen/medium.h>

NAMESPACE_BEGIN(kazen)

/// normals
class NormalIntegrator : public Integrator {
public:
    NormalIntegrator( const PropertyList &props) {
        /* No parameters this time */
        // LOG("intergrator: normal");
    }

    Color3f Li(const Scene *scene,  Sampler *sampler, const Ray3f &ray) const {
        /* Find the surface that is visible in the requested direction */
        Intersection its;
        if (!scene->rayIntersect(ray, its))
            return Color3f(0.f);

        /* Return the component-wise absolute
           value of the shading normal as a color */
        // Normal3f n = its.shFrame.n.cwiseAbs();
        Normal3f n = its.geoFrame.n.cwiseAbs();
        return Color3f(n.x(), n.y(), n.z());
    }

    std::string toString() const {
        return "NormalIntegrator[]";
    }
};

/// ao
class AmbientOcclusionIntegrator : public Integrator {
public:
    AmbientOcclusionIntegrator( const PropertyList &props) {

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
    WhittedIntegrator( const PropertyList& props) {}
    
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
            Color3f Ls = light->getLight()->sample(rec, sampler, light);
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
            Color3f reflect = its.mesh->getBSDF()->sample(bRec, sampler->next1D(), sampler->next2D());
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


class PathMatsIntegrator : public Integrator {
public:
    PathMatsIntegrator( const PropertyList &props) {}

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray) const override {
        Color3f color = 0;
        Color3f t = 1;
        Ray3f rayRecursive = ray;
        float probability;

        while (true) {
            Intersection its;
            if (!scene->rayIntersect(rayRecursive, its))
                return color;

            //contribute emitted
            if (its.mesh->isLight()) {
                LightQueryRecord lRecE(rayRecursive.o, its.p, its.shFrame.n);
                color += t * its.mesh->getLight()->eval(lRecE);
            }

            //Russian roulettio
            probability = std::min(t.x(), 0.95f);
            if (sampler->next1D() >= probability)
                return color;

            t /= probability;

            //BSDF
            BSDFQueryRecord bRec(its.shFrame.toLocal(-rayRecursive.d));
            bRec.uv = its.uv;
            Color3f f = its.mesh->getBSDF()->sample(bRec, sampler->next1D(), sampler->next2D());
            t *= f;

           //continue recursion
           rayRecursive = Ray3f(its.p, its.toWorld(bRec.wo));
        }

        return color;
    }

    std::string toString() const {
        return "PathMatsIntegrator[]";
    }
};


// Next Event Estimation(NEE) with multiple importance sampling(MIS)
class PathMisIntegrator : public Integrator {
public:
    PathMisIntegrator(const PropertyList &propList) {
        // system support 512 max bounces
        m_maxDepth = std::min(512, propList.getInteger("maxDepth", 5));
        m_rayEpsilon = propList.getFloat("traceBias", 0.001f);
        m_regularization = propList.getBoolean("regularization", false);
        m_accumulatedRoughness = propList.getFloat("accumulatedRoughness", 0.5f);
    }

    Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray_) const {
        Ray3f ray = ray_;
        Color3f Li(0.f), throughput(1.f);
        
        /* Tracks radiance scaling due to index of refraction changes */
        float eta = 1.f;

        /* MIS weight for intersected lights (set by prev. iteration) */
        float bsdfWeight(1.f);

        /* Rougness bias: //https://twitter.com/YuriyODonnell/status/1199253959086612480 */
        // float roughnessBias(0.f);

        /* First intersection */
        Intersection its;
        if (!scene->rayIntersect(ray, its)) {
            return Li;
        } 
        else {
            if (its.mesh->isLight()) {
                if (!its.mesh->getLight()->getPrimaryVisibility()) {
                    Ray3f newRay = Ray3f(its.p + m_rayEpsilon*ray.d, ray.d);
                    scene->rayIntersect(newRay, its);
                }
            }
        }
    
        /* Tracks depth for Russian roulette */
        int depth = 0;
        while (depth < m_maxDepth) {
            /* ----------------------- Intersection with lights ----------------------- */
            if (its.mesh->isLight()) {
                LightQueryRecord lRec(ray.o, its.p, its.shFrame.n);
                lRec.uv = its.uv;
                Li += bsdfWeight * throughput * its.mesh->getLight()->eval(lRec);
                break;
            }
      
            /* Russian roulette: try to keep path weights equal to one,
               while accounting for the solid angle compression at refractive
               index boundaries. Stop with at least some probability to avoid
               getting stuck (e.g. due to total internal reflection) */
            if (depth >= 3) {
                // continuation probability
                auto probability = std::min(throughput.maxCoeff()*eta*eta, 0.95f);
                if (probability <= sampler->next1D()) {
                    break;
                }
                throughput /= probability;
            }

            /* ----------------------- Light sampling ----------------------- */
            const Mesh* mesh = scene->getRandomLight(sampler->next1D());
            if (mesh) {
                const Light* light = mesh->getLight();
                LightQueryRecord lRec(its.p);
                lRec.uv = its.uv;
                Color3f Ls = light->sample(lRec, sampler, mesh) / scene->getLightPdf();
                auto lightPdf = light->pdf(lRec, mesh);
            
                /* Apply trace bias to shadow ray. */
                lRec.shadowRay.mint = m_rayEpsilon;
                lRec.shadowRay.maxt -= m_rayEpsilon;

                Intersection shadowIts;
                bool occluded = false;
                auto tempRay = lRec.shadowRay;
                while (true) {
                    if (scene->rayOccluded(tempRay, shadowIts)) {
                        if (!shadowIts.mesh->isLight()) {
                            occluded = true;
                            break;
                        } else {
                            if (shadowIts.mesh->getLight()->getPrimaryVisibility()) {
                                occluded = true;
                                break;
                            }
                            tempRay = Ray3f(tempRay.o + tempRay.d*(shadowIts.t+m_rayEpsilon), tempRay.d, m_rayEpsilon, tempRay.maxt-shadowIts.t);
                        }
                    }
                    else {
                        break;
                    }
                }
        
                // if (!scene->rayOccluded(lRec.shadowRay, shadowIts)) {
                if (!occluded) {
                    /* Query the BSDF for that emitter-sampled direction */
                    BSDFQueryRecord bRec(its.toLocal(-ray.d), its.toLocal(lRec.wi), ESolidAngle);
                    bRec.its = its;
                    bRec.uv = its.uv;

                    Color3f f = its.mesh->getBSDF()->eval(bRec);

                    /* Determine density of sampling that same direction using BSDF sampling */
                    auto bsdfPdf = its.mesh->getBSDF()->pdf(bRec);

                    auto lightWeight = powerHeuristic(lightPdf, bsdfPdf);
                    Li += throughput * Ls * f  * lightWeight;
                }
            }

            /* Regularize the bsdf to reduce firefly issue */
            // if (m_regularization && depth >= 1) {
            if (m_regularization) {
                its.accumulatedRoughness += its.mesh->getBSDF()->regularize(its.uv) * m_accumulatedRoughness;
            }

            /* ----------------------- BSDF sampling ----------------------- */
            BSDFQueryRecord bRec(its.shFrame.toLocal(-ray.d));
            bRec.uv = its.uv;
            bRec.its = its;
            auto bsdfColor = its.mesh->getBSDF()->sample(bRec, sampler->next1D(), sampler->next2D()); // Sample BSDF * cos(theta)
            throughput *= bsdfColor;
            eta *= bRec.eta;

            /*  Intersect the BSDF ray against the scene geometry */
            ray = Ray3f(its.p, its.toWorld(bRec.wo));
            ray.mint = m_rayEpsilon;
            auto bsdfPdf = its.mesh->getBSDF()->pdf(bRec);
            if (!scene->rayIntersect(ray, its)) {
                Li += throughput * scene->getBackgroundColor(ray.d);
                break;
            }

            /* Determine probability of having sampled that same
               direction using emitter sampling. */
            if (its.mesh->isLight()) {
                LightQueryRecord lRec_ = LightQueryRecord(ray.o, its.p, its.shFrame.n);
                lRec_.uv = its.uv;
                float lightPdf_ = its.mesh->getLight()->pdf(lRec_, its.mesh);
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

private:
    int m_maxDepth;
    float m_rayEpsilon;
    bool m_regularization;
    float m_accumulatedRoughness;
};


// /// volpath
// /// TODO: 2 time slower than PathMisIntegrator
// class VolPathIntegrator : public Integrator {
// public:
//     VolPathIntegrator(const PropertyList &propList) {}

//     Color3f Li(const Scene *scene, Sampler *sampler, const Ray3f &ray_) const {
//         Color3f color(0.f);
//         Color3f throughput(1.f);
//         Intersection its;
//         Medium *medium = nullptr;
//         float eta = 1.f;
//         Ray3f ray = ray_;

//         // ??????????????????????????????????????????????????????????????????(maxBounces)?????????
//         const size_t maxBounces = 50;
//         for (size_t bounce = 0; bounce < maxBounces; bounce++) {

//             // ??????ray????????????????????????????????????backgroundColor
//             if (!scene->rayIntersect(ray, its)) {
//                 color += throughput * scene->getBackgroundColor();
//                 break;
//             }

//             // ?????? transmission
//             // TODO: only support beer-lambertian non-scatter medium for now
// 		    if (medium != nullptr) {
//                 float distance = medium->distance(ray, sampler->next2D());
//                 Color3f transmission = medium->transmission(distance);
//                 throughput *= transmission;
//             }

//             // ?????????????????????????????????????????????
//             auto light = its.mesh->getLight();
//             if (bounce == 0 && light != nullptr) {
//                 LightQueryRecord lRec(ray.o, its.p, its.shFrame.n);
//                 color += throughput * light->eval(lRec);
//             }

//             // ??????????????????
//             color += throughput * sampleOneLight(scene, sampler, ray, its);
            
//             // [ISSUE]: ???????????????????????????????????????NEE??????????????????sample???,
//             // ?????????????????????????????????????????????interaction.wo??????eval
//             // ??????????????????pbrt?????????
//             //
//             // ???????????????????????????????????????
//             // ??????????????????bsdf sample??????wo?????????
//             //
//             // ????????????????????????????????????????????????intersection.wo?????????
//             // ????????????pdf???????????????????????????????????????????????????
//             BSDFQueryRecord bRec(its.shFrame.toLocal(-ray.d));
//             bRec.uv = its.uv; // ?????????uv????????????????????????????????????????????????
//             // its.mesh->getBSDF()->sample(bRec, sampler->next2D());
//             // float pdf = its.mesh->getBSDF()->pdf(bRec);

//             // ??????throughput???throughput????????????????????????
//             // ?????????????????????????????????(w1 -> w2)????????????
//             //
//             // ?????????????????????(w0->w1->w2...->wn)?????????????????????w0????????????wn?????????????????????
//             // ??????pbr????????????????????????????????????wn??????w(n-1)????????????eye?????????
//             // ????????????????????????-?????????-?????????????????????????????????w(n-1)??????????????????wn???
//             // w(n-2)??????????????????w(n-1) ... w0??????????????????w1???
//             // ????????????????????????????????????????????????????????????????????????
//             // ?????????????????????????????????t(??????) = t(?????????) * ?????????????????????;
//             //
//             // ?????????????????????sample????????????return???color???color=eval/pdf???
//             // ????????????????????????sample??????void????????????????????????????????????????????????????????????
//             // throughput *= its.mesh->getBSDF()->eval(bRec)/pdf; 
//             throughput *= its.mesh->getBSDF()->sample(bRec, sampler->next2D());
//             // eta *= bRec.eta;

//             // ??????intersection???p???wo??????ray?????????
//             // ray.o = its.p;
//             // ray.d = its.toWorld(bRec.wo);
//             // ray.mint = 0.001f;
//             // ray.maxt = std::numeric_limits<float>::infinity();                    
//             ray = Ray3f(its.p, its.toWorld(bRec.wo));

//             // Russian roulette
//             if (bounce > 3) {
//                 // continuation probability(p)
//                 auto p = std::min(throughput.maxCoeff()*eta*eta, 0.95f);
//                 if (p <= sampler->next1D()) {
//                     break;
//                 }
//                 throughput /= p;
//             }
//         }

//         return color;
//     }

//     std::string toString() const {
//         return "VolPathIntegrator[]";
//     }

// private:

//     Color3f sampleOneLight(const Scene *scene, Sampler *sampler, 
//         const Ray3f &ray, Intersection &its) const {
        
//         size_t numLights = scene->getNumLights();

//         // Return black if there are no lights
//         // And don't let a light contribute light to itself
//         // Aka, if we hit a light
//         // This is the special case where there is only 1 light
//         if (numLights == 0 || numLights == 1 && its.mesh->getLight() != nullptr) {
//             return Color3f(0.f);
//         }

//         // Don't let a light contribute light to itself
//         // Choose another one
//         Mesh *lightMesh;
//         do {
//             lightMesh = scene->getRandomLight(sampler->next1D());
//         } while (lightMesh == its.mesh);

//         return numLights * estimateDirect(scene, sampler, ray, its, lightMesh);        
//     }

//     /// next event estimation
//     Color3f estimateDirect(const Scene *scene, Sampler *sampler, 
//         const Ray3f &ray, Intersection &its, Mesh *lightMesh) const {
        
//         Color3f directLighting = Color3f(0.f);
//         Color3f f; // brdf
//         float lightPdf, bsdfPdf;
//         Light *light = lightMesh->getLight();

//         // Sample lighting with multiple importance sampling
//         LightQueryRecord lRec(its.p);
//         Color3f Li = light->sample(lightMesh, lRec, sampler);
//         lightPdf = light->pdf(lightMesh, lRec);

//         // Make sure the pdf isn't zero and the radiance isn't black
//         if (lightPdf != 0.f && Li.all()) {
//             // TODO: We should move this intersection test to light sampleLi()
//             if (!scene->rayIntersect(lRec.shadowRay)) {

//                 BSDFQueryRecord bRec(its.toLocal(-ray.d), its.toLocal(lRec.wi), ESolidAngle);    
//                 bRec.uv = its.uv;
//                 // Calculate the brdf value
//                 f = its.mesh->getBSDF()->eval(bRec);
//                 bsdfPdf = its.mesh->getBSDF()->pdf(bRec);

//                 if (bsdfPdf != 0.f && f.array().all()) {
//                     float weight = powerHeuristic(1, lightPdf, 1, bsdfPdf);
//                     // ??????????????????????????????(f*Li/lightPdf) * weight
//                     // [ISSUE]: mitsuba???????????????sampleLight =  f * Li * weight
//                     // ?????????????????????lightPdf??????????????????
//                     // [Resolve]: ??????sample????????????lightPdf??????????????????????????????
//                     // directLighting += f * Li * weight / lightPdf;
//                     directLighting += f * Li * weight;
//                 }
//             }
//         }

//         // Sample brdf with multiple importance sampling
//         BSDFQueryRecord bRec_(its.shFrame.toLocal(-ray.d));
//         bRec_.uv = its.uv;
//         f = its.mesh->getBSDF()->sample(bRec_, sampler->next2D());
//         // f = its.mesh->getBSDF()->eval(bRec_);
//         bsdfPdf = its.mesh->getBSDF()->pdf(bRec_);

//         if (bsdfPdf != 0.f && f.array().all()) {
//             Intersection its_;
//             Ray3f ray_ = Ray3f(its.p, its.toWorld(bRec_.wo));
//             if (!scene->rayIntersect(ray_, its_)) 
//                 return directLighting;

//             if (its_.mesh->isLight()) {
//                 LightQueryRecord lRec_ = LightQueryRecord(ray_.o, its_.p, its_.shFrame.n);

//                 lightPdf = its_.mesh->getLight()->pdf(its_.mesh, lRec_);
//                 float weight = powerHeuristic(1, bsdfPdf, 1, lightPdf);
            
//                 Color3f Li = its_.mesh->getLight()->eval(lRec_);
//                 directLighting += f * Li * weight;
//             }
//         }

//         return directLighting;
//     }

//     float powerHeuristic(uint numf, float fPdf, uint numg, float gPdf) const {
//         float f = numf * fPdf;
//         float g = numg * gPdf;

//         return (f * f) / (f * f + g * g);
//     }

// };


KAZEN_REGISTER_CLASS(NormalIntegrator, "normals");
KAZEN_REGISTER_CLASS(AmbientOcclusionIntegrator, "ao");
KAZEN_REGISTER_CLASS(WhittedIntegrator, "whitted");
KAZEN_REGISTER_CLASS(PathMatsIntegrator, "path_mats");
KAZEN_REGISTER_CLASS(PathMisIntegrator, "path_mis");
// KAZEN_REGISTER_CLASS(VolPathIntegrator, "path_vol");
NAMESPACE_END(kazen)