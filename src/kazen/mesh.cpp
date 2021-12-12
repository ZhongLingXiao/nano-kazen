#include <kazen/common.h>
#include <kazen/mesh.h>
#include <kazen/bbox.h>
#include <kazen/bsdf.h>
#include <kazen/light.h>
#include <kazen/warp.h>
#include <kazen/timer.h>
#include <Eigen/Geometry>
#include <filesystem/resolver.h>
#include <unordered_map>
#include <fstream>


NAMESPACE_BEGIN(kazen)

Mesh::Mesh() { }

Mesh::~Mesh() {
    delete m_bsdf;
    delete m_light;
    delete m_dpdf;
}

void Mesh::activate() {
    if (!m_bsdf) {
        /* If no material was assigned, instantiate a diffuse BRDF */
        m_bsdf = static_cast<BSDF *>(ObjectFactory::createInstance("diffuse", PropertyList()));
    }

    //check if the mesh is an emitter
    if(isLight()){
        m_dpdf = new DiscretePDF(m_F.cols());

        //for every face set the surface area
        m_dpdf->reserve(m_F.cols());
        for (int i = 0; i < m_F.cols(); ++i) {
            auto area = surfaceArea(i);
            m_dpdf->append(area);
            m_area += area;
        }

        //normalize the pdf
        m_dpdf->normalize();
    }
}

float Mesh::surfaceArea(uint32_t index) const {
    uint32_t i0 = m_F(0, index), i1 = m_F(1, index), i2 = m_F(2, index);

    const Point3f p0 = m_V.col(i0), p1 = m_V.col(i1), p2 = m_V.col(i2);

    return 0.5f * Vector3f((p1 - p0).cross(p2 - p0)).norm();
}

bool Mesh::rayIntersect(uint32_t index, const Ray3f &ray, float &u, float &v, float &t) const {
    uint32_t i0 = m_F(0, index), i1 = m_F(1, index), i2 = m_F(2, index);
    const Point3f p0 = m_V.col(i0), p1 = m_V.col(i1), p2 = m_V.col(i2);

    /* Find vectors for two edges sharing v[0] */
    Vector3f edge1 = p1 - p0, edge2 = p2 - p0;

    /* Begin calculating determinant - also used to calculate U parameter */
    Vector3f pvec = ray.d.cross(edge2);

    /* If determinant is near zero, ray lies in plane of triangle */
    float det = edge1.dot(pvec);

    if (det > -1e-8f && det < 1e-8f)
        return false;
    float inv_det = 1.0f / det;

    /* Calculate distance from v[0] to ray origin */
    Vector3f tvec = ray.o - p0;

    /* Calculate U parameter and test bounds */
    u = tvec.dot(pvec) * inv_det;
    if (u < 0.0 || u > 1.0)
        return false;

    /* Prepare to test V parameter */
    Vector3f qvec = tvec.cross(edge1);

    /* Calculate V parameter and test bounds */
    v = ray.d.dot(qvec) * inv_det;
    if (v < 0.0 || u + v > 1.0)
        return false;

    /* Ray intersects triangle -> compute t */
    t = edge2.dot(qvec) * inv_det;

    return t >= ray.mint && t <= ray.maxt;
}

BoundingBox3f Mesh::getBoundingBox(uint32_t index) const {
    BoundingBox3f result(m_V.col(m_F(0, index)));
    result.expandBy(m_V.col(m_F(1, index)));
    result.expandBy(m_V.col(m_F(2, index)));
    return result;
}

Point3f Mesh::getCentroid(uint32_t index) const {
    return (1.0f / 3.0f) *
        (m_V.col(m_F(0, index)) +
         m_V.col(m_F(1, index)) +
         m_V.col(m_F(2, index)));
}

void Mesh::sample(Sampler *sampler, Point3f &p, Normal3f &n) const {
    auto index = m_dpdf->sample(sampler->next1D());
    
    /* sample a barycentric coordinate: pbrt-13.6.5 Sampling a Triangle
    https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations#SamplingaUnitDisk
    */
    float sqrtOneMinusEpsilon = std::sqrt(1-sampler->next1D());
    float alpha = 1 - sqrtOneMinusEpsilon ;
    float beta = sampler->next1D() * sqrtOneMinusEpsilon;

    uint32_t i0 = m_F(0, index), i1 = m_F(1, index), i2 = m_F(2, index);
    
    /* position */
    const Point3f p0 = m_V.col(i0), p1 = m_V.col(i1), p2 = m_V.col(i2);
    p = p0 + alpha * (p1 - p0) + beta * (p2 - p0);

    /* normal */
	if (m_N.size() > 0) {
		const Normal3f n0 = m_N.col(i0), n1 = m_N.col(i1), n2 = m_N.col(i2);
		n = n0 + alpha * (n1 - n0) + beta * (n2 - n0);
        n.normalized();
	} else {
		n = (p1 - p0).cross(p2 - p0).normalized();
	} 
}


void Mesh::addChild(Object *obj) {
    switch (obj->getClassType()) {
        case EBSDF:
            if (m_bsdf)
                throw Exception("Mesh: tried to register multiple BSDF instances!");
            m_bsdf = static_cast<BSDF *>(obj);
            break;

        case ELight: {
                Light *light = static_cast<Light *>(obj);
                if (m_light)
                    throw Exception("Mesh: tried to register multiple Light instances!");
                m_light = light;
            }
            break;

        default:
            throw Exception("Mesh::addChild(<{}>) is not supported!", classTypeName(obj->getClassType()));
    }
}

std::string Mesh::toString() const {
    return fmt::format(
        "Mesh[\n"
        "  name = \"{}\",\n"
        "  vertexCount = {},\n"
        "  triangleCount = {},\n"
        "  bsdf = {},\n"
        "  light = {}\n"
        "]",
        m_name,
        m_V.cols(),
        m_F.cols(),
        m_bsdf ? string::indent(m_bsdf->toString()) : std::string("null"),
        m_light ? string::indent(m_light->toString()) : std::string("null")
    );
}

std::string Intersection::toString() const {
    if (!mesh)
        return "Intersection[invalid]";

    return fmt::format(
        "Intersection[\n"
        "  p = {},\n"
        "  t = {},\n"
        "  uv = {},\n"
        "  shFrame = {},\n"
        "  geoFrame = {},\n"
        "  mesh = {}\n"
        "]",
        p.toString(),
        t,
        uv.toString(),
        string::indent(shFrame.toString()),
        string::indent(geoFrame.toString()),
        mesh ? mesh->toString() : std::string("null")
    );
}


/**
 * \brief Loader for Wavefront OBJ triangle meshes
 */
class WavefrontOBJ : public Mesh {
public:
    WavefrontOBJ(const PropertyList &propList) {
        typedef std::unordered_map<OBJVertex, uint32_t, OBJVertexHash> VertexMap;

        filesystem::path filename = getFileResolver()->resolve(propList.getString("filename"));

        std::ifstream is(filename.str());
        if (is.fail())
            throw Exception("Unable to open OBJ file \"{}\"!", filename.str());
        Transform trafo = propList.getTransform("toWorld", Transform());

        // cout << "Loading \"" << filename << "\" ==> ";
        // LOG("Loading Mesh: \"{}\" ... ", filename.str());
        cout.flush();
        Timer timer;

        std::vector<Vector3f>   positions;
        std::vector<Vector2f>   texcoords;
        std::vector<Vector3f>   normals;
        std::vector<uint32_t>   indices;
        std::vector<OBJVertex>  vertices;
        VertexMap vertexMap;

        std::string line_str;
        while (std::getline(is, line_str)) {
            std::istringstream line(line_str);

            std::string prefix;
            line >> prefix;

            if (prefix == "v") {
                Point3f p;
                line >> p.x() >> p.y() >> p.z();
                p = trafo * p;
                m_bbox.expandBy(p);
                positions.push_back(p);
            } else if (prefix == "vt") {
                Point2f tc;
                line >> tc.x() >> tc.y();
                texcoords.push_back(tc);
            } else if (prefix == "vn") {
                Normal3f n;
                line >> n.x() >> n.y() >> n.z();
                normals.push_back((trafo * n).normalized());
            } else if (prefix == "f") {
                std::string v1, v2, v3, v4;
                line >> v1 >> v2 >> v3 >> v4;
                OBJVertex verts[6];
                int nVertices = 3;

                verts[0] = OBJVertex(v1);
                verts[1] = OBJVertex(v2);
                verts[2] = OBJVertex(v3);

                if (!v4.empty()) {
                    /* This is a quad, split into two triangles */
                    verts[3] = OBJVertex(v4);
                    verts[4] = verts[0];
                    verts[5] = verts[2];
                    nVertices = 6;
                }
                /* Convert to an indexed vertex list */
                for (int i=0; i<nVertices; ++i) {
                    const OBJVertex &v = verts[i];
                    VertexMap::const_iterator it = vertexMap.find(v);
                    if (it == vertexMap.end()) {
                        vertexMap[v] = (uint32_t) vertices.size();
                        indices.push_back((uint32_t) vertices.size());
                        vertices.push_back(v);
                    } else {
                        indices.push_back(it->second);
                    }
                }
            }
        }

        m_F.resize(3, indices.size()/3);
        memcpy(m_F.data(), indices.data(), sizeof(uint32_t)*indices.size());

        m_V.resize(3, vertices.size());
        for (uint32_t i=0; i<vertices.size(); ++i)
            m_V.col(i) = positions.at(vertices[i].p-1);

        if (!normals.empty()) {
            m_N.resize(3, vertices.size());
            for (uint32_t i=0; i<vertices.size(); ++i)
                m_N.col(i) = normals.at(vertices[i].n-1);
        }

        if (!texcoords.empty()) {
            m_UV.resize(2, vertices.size());
            for (uint32_t i=0; i<vertices.size(); ++i)
                m_UV.col(i) = texcoords.at(vertices[i].uv-1);
        }

        m_name = filename.str();
        // cout << "done. (V=" << m_V.cols() << ", F=" << m_F.cols() << ", took "
        //      << timer.elapsedString() << " and "
        //      << util::memString(m_F.size() * sizeof(uint32_t) +
        //                   sizeof(float) * (m_V.size() + m_N.size() + m_UV.size()))
        //      << ")" << endl;
        LOG("Mesh ready.    (took {}): \"{}\"", util::timeString(timer.elapsed()), filename.str());
    }

protected:
    /// Vertex indices used by the OBJ format
    struct OBJVertex {
        uint32_t p = (uint32_t) -1;
        uint32_t n = (uint32_t) -1;
        uint32_t uv = (uint32_t) -1;

        inline OBJVertex() { }

        inline OBJVertex(const std::string &string) {
            std::vector<std::string> tokens = string::tokenize(string, "/", true);

            if (tokens.size() < 1 || tokens.size() > 3)
                throw Exception("Invalid vertex data: \"{}\"", string);

            p = string::toUInt(tokens[0]);

            if (tokens.size() >= 2 && !tokens[1].empty())
                uv = string::toUInt(tokens[1]);

            if (tokens.size() >= 3 && !tokens[2].empty())
                n = string::toUInt(tokens[2]);
        }

        inline bool operator==(const OBJVertex &v) const {
            return v.p == p && v.n == n && v.uv == uv;
        }
    };

    /// Hash function for OBJVertex
    struct OBJVertexHash {
        std::size_t operator()(const OBJVertex &v) const {
            size_t hash = std::hash<uint32_t>()(v.p);
            hash = hash * 37 + std::hash<uint32_t>()(v.uv);
            hash = hash * 37 + std::hash<uint32_t>()(v.n);
            return hash;
        }
    };
};

KAZEN_REGISTER_CLASS(WavefrontOBJ, "obj");
NAMESPACE_END(kazen)
