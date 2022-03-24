#pragma once

#include <kazen/accel.h>

NAMESPACE_BEGIN(kazen)

/**
 * \brief Main scene data structure
 *
 * This class holds information on scene objects and is responsible for
 * coordinating rendering jobs. It also provides useful query routines that
 * are mostly used by the \ref Integrator implementations.
 */
class Scene : public Object {
public:
    /// Construct a new scene object
    Scene(const PropertyList &);

    /// Release all memory
    virtual ~Scene();

    /// Return a pointer to the scene's kd-tree
    const Accel *getAccel() const { return m_accel; }

    /// Return a pointer to the scene's integrator
    const Integrator *getIntegrator() const { return m_integrator; }

    /// Return a pointer to the scene's integrator
    Integrator *getIntegrator() { return m_integrator; }

    /// Return a pointer to the scene's camera
    const Camera *getCamera() const { return m_camera; }

    /// Return a pointer to the scene's sample generator (const version)
    const Sampler *getSampler() const { return m_sampler; }

    /// Return a pointer to the scene's sample generator
    Sampler *getSampler() { return m_sampler; }

    /// Return a reference to an array containing all meshes
    const std::vector<Mesh *> &getMeshes() const { return m_meshes; }

    /// Return a random light mesh
    Mesh *getRandomLight(float rnd) const {         
        auto const & n = m_lights.size();
        size_t index = std::min(
                static_cast<size_t>(std::floor(n*rnd)),
                n-1);
        return m_lights[index]; 
    }

    /// Return a light's pdf
    float getLightPdf() const { return 1.f/m_lights.size(); }

    const std::vector<Mesh *> &getLights() const { return m_lights; }

    size_t getNumLights() const { return m_lights.size(); }

    /// Return background color
    const Color3f getBackgroundColor() const { return m_backgroundColor; }

    /**
     * \brief Intersect a ray against all triangles stored in the scene
     * and return detailed intersection information
     *
     * \param ray
     *    A 3-dimensional ray data structure with minimum/maximum
     *    extent information
     *
     * \param its
     *    A detailed intersection record, which will be filled by the
     *    intersection query
     *
     * \return \c true if an intersection was found
     */
    bool rayIntersect(const Ray3f &ray, Intersection &its) const {
        return m_accel->rayIntersect(ray, its, false);
    }

    /**
     * \brief Intersect a ray against all triangles stored in the scene
     * and \a only determine whether or not there is an intersection.
     *
     * This method much faster than the other ray tracing function,
     * but the performance comes at the cost of not providing any
     * additional information about the detected intersection
     * (not even its position).
     *
     * \param ray
     *    A 3-dimensional ray data structure with minimum/maximum
     *    extent information
     *
     * \return \c true if an intersection was found
     */
    bool rayIntersect(const Ray3f &ray) const {
        Intersection its; /* Unused */
        return m_accel->rayIntersect(ray, its, true);
    }

    /// \brief Return an axis-aligned box that bounds the scene
    const BoundingBox3f &getBoundingBox() const {
        return m_accel->getBoundingBox();
    }

    /**
     * \brief Inherited from \ref Object::activate()
     *
     * Initializes the internal data structures (accel build,
     * light sampling data structures, etc.)
     */
    void activate();

    /// Add a child object to the scene (meshes, integrators etc.)
    void addChild(Object *obj);

    /// Return a string summary of the scene (for debugging purposes)
    std::string toString() const;

    EClassType getClassType() const { return EScene; }
private:
    std::vector<Mesh *> m_meshes;
    std::vector<Mesh *> m_lights;
    Integrator *m_integrator = nullptr;
    Sampler *m_sampler = nullptr;
    Camera *m_camera = nullptr;
    Accel *m_accel = nullptr;
    Color3f m_backgroundColor = Color3f(0.05f);
};

NAMESPACE_END(kazen)
