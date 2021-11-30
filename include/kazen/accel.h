#pragma once

#include <kazen/mesh.h>
#include <embree3/rtcore.h>

NAMESPACE_BEGIN(kazen)

/**
 * \brief Acceleration data structure for ray intersection queries
 *
 * using embree3 for fast intersection test.
 * 
 */
class Accel {
public:

    /// Release all resources
    virtual ~Accel() { clear(); };
    
    /// Release all resources
    void clear();

    /**
     * \brief Register a triangle mesh for inclusion in the acceleration
     * data structure
     *
     * This function can only be used before \ref build() is called
     */
    void addMesh(Mesh *mesh);

    /// Build the acceleration data structure (currently a no-op)
    void build();

    /// Return an axis-aligned box that bounds the scene
    const BoundingBox3f &getBoundingBox() const { return m_bbox; }

    /**
     * \brief Intersect a ray against all triangles stored in the scene and
     * return detailed intersection information
     *
     * \param ray
     *    A 3-dimensional ray data structure with minimum/maximum extent
     *    information
     *
     * \param its
     *    A detailed intersection record, which will be filled by the
     *    intersection query
     *
     * \param shadowRay
     *    \c true if this is a shadow ray query, i.e. a query that only aims to
     *    find out whether the ray is blocked or not without returning detailed
     *    intersection information.
     *
     * \return \c true if an intersection was found
     */
    bool rayIntersect(const Ray3f &ray, Intersection &its, bool shadowRay) const;

private:
    std::vector<Mesh *> m_meshes;                   ///< Meshes 
    BoundingBox3f       m_bbox;                     ///< Bounding box of the entire scene
    /// embree3 related
    RTCDevice   m_device = nullptr;
    RTCScene    m_scene = nullptr;
};

NAMESPACE_END(kazen)
