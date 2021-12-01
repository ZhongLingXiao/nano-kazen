#include <kazen/accel.h>
#include <kazen/timer.h>
#include <Eigen/Geometry>

NAMESPACE_BEGIN(kazen)

void Accel::clear() {
    for (auto &mesh : m_meshes)
        delete mesh;
    m_meshes.clear();

    rtcReleaseScene(m_scene); 
    m_scene = nullptr;

    rtcReleaseDevice(m_device); 
    m_device = nullptr;
}

void Accel::addMesh(Mesh *mesh) {
    m_meshes.push_back(mesh);
}

void Accel::build() {
    Timer timer;
    LOG("================");
    /* create new Embree device */
    m_device = rtcNewDevice(nullptr); // "verbose=1"

    /* create scene */
    m_scene = rtcNewScene(m_device);

    /* add meshes */
    unsigned int geomID = 0;
    for (auto &mesh : m_meshes) {
        RTCGeometry geom = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_TRIANGLE);

        /* fill in geom's vertex and index buffer here */
        float* vb = (float*) rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3*sizeof(float), mesh->getVertexCount());
        for (uint32_t i=0; i<mesh->getVertexCount(); ++i) {
            vb[3*i+0] = mesh->getVertexPositions().col(i).x(); 
            vb[3*i+1] = mesh->getVertexPositions().col(i).y(); 
            vb[3*i+2] = mesh->getVertexPositions().col(i).z();
        }

        unsigned* ib = (unsigned*) rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3*sizeof(unsigned), mesh->getTriangleCount());
        for (uint32_t i=0; i<mesh->getTriangleCount(); ++i) {
            ib[3*i+0] = mesh->getIndices()(0, i); 
            ib[3*i+1] = mesh->getIndices()(1, i); 
            ib[3*i+2] = mesh->getIndices()(2, i);
        }

        /* set id for each geometry */
        rtcCommitGeometry(geom);
        rtcAttachGeometryByID(m_scene, geom, geomID);
        rtcReleaseGeometry(geom);

        /* increase this shit */
        geomID++;
    }
    
    /* commit changes to scene */
    rtcCommitScene(m_scene);

    LOG("Embree ready. (took {})", util::timeString(timer.elapsed()));
}

bool Accel::rayIntersect(const Ray3f &ray_, Intersection &its, bool shadowRay) const {
    bool foundIntersection = false;  // Was an intersection found so far?
    uint32_t f = (uint32_t) -1;      // Triangle index of the closest intersection

    Ray3f ray(ray_); /// Make a copy of the ray (we will need to update its '.maxt' value)

    // /* Brute force search through all triangles */
    // for (uint32_t idx = 0; idx < m_mesh->getTriangleCount(); ++idx) {
    //     float u, v, t;
    //     if (m_mesh->rayIntersect(idx, ray, u, v, t)) {
    //         /* An intersection was found! Can terminate
    //            immediately if this is a shadow ray query */
    //         if (shadowRay)
    //             return true;
    //         ray.maxt = its.t = t;
    //         its.uv = Point2f(u, v);
    //         its.mesh = m_mesh;
    //         f = idx;
    //         foundIntersection = true;
    //     }
    // }

    /* initialize intersect context */
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    /* initialize ray */
    RTCRayHit rayhit; 
    rayhit.ray.org_x = ray.o.x(); 
    rayhit.ray.org_y = ray.o.y(); 
    rayhit.ray.org_z = ray.o.z();
    rayhit.ray.dir_x = ray.d.x(); 
    rayhit.ray.dir_y = ray.d.y(); 
    rayhit.ray.dir_z = ray.d.z();
    rayhit.ray.tnear  = ray.mint;
    rayhit.ray.tfar   = ray.maxt;
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

    /* trace shadow ray */
    if (shadowRay) {
        rtcOccluded1(m_scene, &context, &rayhit.ray);
        if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            return true;
        } else {
            return false;
        }
    }
    
    /* intersect ray with scene */
    rtcIntersect1(m_scene, &context, &rayhit);
    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        ray.maxt = its.t = rayhit.ray.tfar;
        its.uv = Point2f(rayhit.hit.u, rayhit.hit.v);
        its.mesh = m_meshes[rayhit.hit.geomID];
        f = rayhit.hit.primID;
        foundIntersection = true;
    }

    /* post intersection */
    if (foundIntersection) {
        /* At this point, we now know that there is an intersection,
           and we know the triangle index of the closest such intersection.

           The following computes a number of additional properties which
           characterize the intersection (normals, texture coordinates, etc..)
        */

        /* Find the barycentric coordinates */
        Vector3f bary;
        bary << 1-its.uv.sum(), its.uv;

        /* References to all relevant mesh buffers */
        const Mesh *mesh   = its.mesh;
        const MatrixXf &V  = mesh->getVertexPositions();
        const MatrixXf &N  = mesh->getVertexNormals();
        const MatrixXf &UV = mesh->getVertexTexCoords();
        const MatrixXu &F  = mesh->getIndices();

        /* Vertex indices of the triangle */
        uint32_t idx0 = F(0, f), idx1 = F(1, f), idx2 = F(2, f);

        Point3f p0 = V.col(idx0), p1 = V.col(idx1), p2 = V.col(idx2);

        /* Compute the intersection positon accurately
           using barycentric coordinates */
        its.p = bary.x() * p0 + bary.y() * p1 + bary.z() * p2;

        /* Compute proper texture coordinates if provided by the mesh */
        if (UV.size() > 0)
            its.uv = bary.x() * UV.col(idx0) +
                bary.y() * UV.col(idx1) +
                bary.z() * UV.col(idx2);

        /* Compute the geometry frame */
        its.geoFrame = Frame((p1-p0).cross(p2-p0).normalized());

        if (N.size() > 0) {
            /* Compute the shading frame. Note that for simplicity,
               the current implementation doesn't attempt to provide
               tangents that are continuous across the surface. That
               means that this code will need to be modified to be able
               use anisotropic BRDFs, which need tangent continuity */

            its.shFrame = Frame(
                (bary.x() * N.col(idx0) +
                 bary.y() * N.col(idx1) +
                 bary.z() * N.col(idx2)).normalized());
        } else {
            its.shFrame = its.geoFrame;
        }
    }

    return foundIntersection;
}

NAMESPACE_END(kazen)

