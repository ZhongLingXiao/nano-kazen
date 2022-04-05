/*
    This file is part of Nori, a simple educational ray tracer

    Copyright (c) 2015 by Wenzel Jakob
*/

#include <kazen/scene.h>
#include <kazen/bitmap.h>
#include <kazen/integrator.h>
#include <kazen/sampler.h>
#include <kazen/camera.h>
#include <kazen/light.h>

NAMESPACE_BEGIN(kazen)

Scene::Scene(const PropertyList &) {
    m_accel = new Accel();
}

Scene::~Scene() {
    OIIO::TextureSystem::destroy(getTextureSystem());

    delete m_accel;
    delete m_sampler;
    delete m_camera;
    delete m_integrator;
}

void Scene::activate() {
    m_accel->build();

    if (!m_integrator)
        throw Exception("No integrator was specified!");
    if (!m_camera)
        throw Exception("No camera was specified!");
    
    if (!m_sampler) {
        /* Create a default (independent) sampler */
        m_sampler = static_cast<Sampler*>(ObjectFactory::createInstance("independent", PropertyList()));
    }

    for (auto &mesh : m_meshes) {
        if (mesh->isLight()) {
            m_lights.push_back(mesh);
        }
    }

    getTextureSystem();
    // cout << endl;
    // cout << "Configuration: " << toString() << endl;
    // cout << endl;
}

const Color3f Scene::getBackgroundColor(const Vector3f &dir) const {
    if (!m_background)
        return Color3f(0.f);
    
    // // Blinn/Newell Latitude Mapping
    // // https://www.reindelsoftware.com/Documents/Mapping/Mapping.html
    // // TODO: Add more mapping methods
    // auto u = (std::atan2(dir.x(), dir.z()) + M_PI) * INV_TWOPI;
    // auto v = (std::asin(dir.y()) + 0.5f*M_PI ) * INV_PI;
    // auto invalid = [&]() {
    //     return std::isnan(dir.x()) || std::isnan(dir.y()) || std::isnan(dir.z()) ||
    //         std::isnan(u) || std::isnan(v);
    // };    
    // if (invalid())
    //     return Color3f(0.f);
    // return m_background->eval(Point2f(u, v));

    auto invalid = [&]() {
        return std::isnan(dir.x()) || std::isnan(dir.y()) || std::isnan(dir.z());
    }; 

    if (invalid())
        return Color3f(0.f);  

    return m_background->eval(dir);
}

void Scene::addChild(Object *obj) {
    switch (obj->getClassType()) {
        case EMesh: {
                Mesh *mesh = static_cast<Mesh *>(obj);
                m_accel->addMesh(mesh);
                m_meshes.push_back(mesh);
            }
            break;
        
        case ELight: {
                //Lights *light = static_cast<Lights *>(obj);
                /* TBD */
                throw Exception("Scene::addChild(): You need to implement this for lights");
            }
            break;

        case ESampler:
            if (m_sampler)
                throw Exception("There can only be one sampler per scene!");
            m_sampler = static_cast<Sampler *>(obj);
            break;

        case ECamera:
            if (m_camera)
                throw Exception("There can only be one camera per scene!");
            m_camera = static_cast<Camera *>(obj);
            break;
        
        case EIntegrator:
            if (m_integrator)
                throw Exception("There can only be one integrator per scene!");
            m_integrator = static_cast<Integrator *>(obj);
            break;

        case ETexture:
            if( obj->getId() == "background" ) {
                if (m_background)
                    throw Exception("There is already an baseColor defined!");
                m_background = static_cast<Texture<Color3f> *>(obj);
            }
            break;
        default:
            throw Exception("Scene::addChild(<{}>) is not supported!",
                classTypeName(obj->getClassType()));
    }
}

std::string Scene::toString() const {
    std::string meshes;
    for (size_t i=0; i<m_meshes.size(); ++i) {
        meshes += std::string("  ") + string::indent(m_meshes[i]->toString(), 2);
        if (i + 1 < m_meshes.size())
            meshes += ",";
        meshes += "\n";
    }

    return fmt::format(
        "Scene[\n"
        "  integrator = {},\n"
        "  sampler = {}\n"
        "  camera = {},\n"
        "  meshes = [\n"
        "  {}  ]\n"
        "]",
        string::indent(m_integrator->toString()),
        string::indent(m_sampler->toString()),
        string::indent(m_camera->toString()),
        string::indent(meshes, 2)
    );
}

KAZEN_REGISTER_CLASS(Scene, "scene");
NAMESPACE_END(kazen)
