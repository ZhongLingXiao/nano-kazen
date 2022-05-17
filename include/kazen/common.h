#pragma once

// std
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <stdint.h>
#include <limits>
#include <cmath>
// 3rd
#include <fmt/core.h>
#include <Eigen/Core>
#include <OpenImageIO/texture.h>
// define
#include <kazen/define.h>


/* "Ray epsilon": relative error threshold for ray intersection computations */
#define Epsilon 1e-5f

/* A few useful constants */
#undef M_PI

#define M_PI         3.14159265358979323846f
#define INV_PI       0.31830988618379067154f
#define INV_TWOPI    0.15915494309189533577f
#define INV_FOURPI   0.07957747154594766788f
#define SQRT_TWO     1.41421356237309504880f
#define INV_SQRT_TWO 0.70710678118654752440f

/* Forward declarations */
namespace filesystem {
    class path;
    class resolver;
};

NAMESPACE_BEGIN(kazen)

/* Forward declarations */
template <typename Scalar, int Dimension>  struct TVector;
template <typename Scalar, int Dimension>  struct TPoint;
template <typename Point, typename Vector> struct TRay;
template <typename Point>                  struct TBoundingBox;

/* Basic kazen data structures (vectors, points, rays, bounding boxes,
   kd-trees) are oblivious to the underlying data type and dimension.
   The following list of typedefs establishes some convenient aliases
   for specific types. 
*/
typedef TVector<float, 1>       Vector1f;
typedef TVector<float, 2>       Vector2f;
typedef TVector<float, 3>       Vector3f;
typedef TVector<float, 4>       Vector4f;
typedef TVector<double, 1>      Vector1d;
typedef TVector<double, 2>      Vector2d;
typedef TVector<double, 3>      Vector3d;
typedef TVector<double, 4>      Vector4d;
typedef TVector<int, 1>         Vector1i;
typedef TVector<int, 2>         Vector2i;
typedef TVector<int, 3>         Vector3i;
typedef TVector<int, 4>         Vector4i;
typedef TPoint<float, 1>        Point1f;
typedef TPoint<float, 2>        Point2f;
typedef TPoint<float, 3>        Point3f;
typedef TPoint<float, 4>        Point4f;
typedef TPoint<double, 1>       Point1d;
typedef TPoint<double, 2>       Point2d;
typedef TPoint<double, 3>       Point3d;
typedef TPoint<double, 4>       Point4d;
typedef TPoint<int, 1>          Point1i;
typedef TPoint<int, 2>          Point2i;
typedef TPoint<int, 3>          Point3i;
typedef TPoint<int, 4>          Point4i;
typedef TBoundingBox<Point1f>   BoundingBox1f;
typedef TBoundingBox<Point2f>   BoundingBox2f;
typedef TBoundingBox<Point3f>   BoundingBox3f;
typedef TBoundingBox<Point4f>   BoundingBox4f;
typedef TBoundingBox<Point1d>   BoundingBox1d;
typedef TBoundingBox<Point2d>   BoundingBox2d;
typedef TBoundingBox<Point3d>   BoundingBox3d;
typedef TBoundingBox<Point4d>   BoundingBox4d;
typedef TBoundingBox<Point1i>   BoundingBox1i;
typedef TBoundingBox<Point2i>   BoundingBox2i;
typedef TBoundingBox<Point3i>   BoundingBox3i;
typedef TBoundingBox<Point4i>   BoundingBox4i;
typedef TRay<Point2f, Vector2f> Ray2f;
typedef TRay<Point3f, Vector3f> Ray3f;

/// Some more forward declarations
class BSDF;
class Bitmap;
class BlockGenerator;
class Camera;
class ImageBlock;
class Integrator;
class Light;
struct LightQueryRecord;
class Mesh;
class Object;
class ObjectFactory;
class PhaseFunction;
class ReconstructionFilter;
class Sampler;
class Scene;

/// Import cout, cerr, endl for debugging purposes
using std::cout;
using std::cerr;
using std::endl;

typedef Eigen::Matrix<float,    Eigen::Dynamic, Eigen::Dynamic> MatrixXf;
typedef Eigen::Matrix<uint32_t, Eigen::Dynamic, Eigen::Dynamic> MatrixXu;

/// Simple exception class, which stores a human-readable error description
class Exception : public std::runtime_error {
public:
    /// Variadic template constructor to support printf-style arguments
    template <typename... Args> Exception(const char *fmt, const Args &... args) 
     : std::runtime_error(fmt::format(fmt, args...)) { }
};


/// util
NAMESPACE_BEGIN(util)
    /// Return the number of cores (real and virtual)
    extern int getCoreCount();

    /// Determine the width of the terminal window that is used to run kazen
    extern int terminalWidth();
 
    /// Return human-readable information about the version
    extern std::string copyright();    

    /// Convert a time value in milliseconds into a human-readable string
    extern std::string timeString(double time, bool precise = false);

    /// Convert a memory amount in bytes into a human-readable string
    extern std::string memString(size_t size, bool precise = false);

    /// Convert a current time <HH:mm:ss> into a human-readable string
    extern std::string currentTime();

NAMESPACE_END(util)


/// string
NAMESPACE_BEGIN(string)

    /// Indent a string by the specified number of spaces
    extern std::string indent(const std::string &string, int amount = 2);

    /// Convert a string to lower case
    extern std::string toLower(const std::string &value);

    /// Convert a string into an boolean value
    extern bool toBool(const std::string &str);

    /// Convert a string into a signed integer value
    extern int toInt(const std::string &str);

    /// Convert a string into an unsigned integer value
    extern unsigned int toUInt(const std::string &str);

    /// Convert a string into a floating point value
    extern float toFloat(const std::string &str);

    /// Convert a string into a 3D vector
    extern Eigen::Vector3f toVector3f(const std::string &str);

    /// Tokenize a string into a list by splitting at 'delim'
    extern std::vector<std::string> tokenize(const std::string &s, const std::string &delim = ", ", bool includeEmpty = false);

    /// Check if a string ends with another string
    extern bool endsWith(const std::string &value, const std::string &ending);

NAMESPACE_END(string)


/// math
NAMESPACE_BEGIN(math)

    //// Convert radians to degrees
    inline float radToDeg(float value) { return value * (180.0f / M_PI); }

    /// Convert degrees to radians
    inline float degToRad(float value) { return value * (M_PI / 180.0f); }

    // #if !defined(_GNU_SOURCE)
    //     /// Emulate sincosf using sinf() and cosf()
    //     inline void sincosf(float theta, float *_sin, float *_cos) {
    //         *_sin = sinf(theta);
    //         *_cos = cosf(theta);
    //     }
    // #endif
    inline void sincosf(float theta, float *_sin, float *_cos) {
            *_sin = std::sin(theta);
            *_cos = std::cos(theta);
    }

    /// Simple floating point clamping function
    inline float clamp(float value, float min, float max) {
        if (value < min)
            return min;
        else if (value > max)
            return max;
        else return value;
    }

    /// Simple integer clamping function
    inline int clamp(int value, int min, int max) {
        if (value < min)
            return min;
        else if (value > max)
            return max;
        else return value;
    }

    /// Linearly interpolate between two values
    inline float lerp(float t, float v1, float v2) {
        return (1.f - t) * v1 + t * v2;
    }

    /// Always-positive modulo operation
    inline int mod(int a, int b) {
        int r = a % b;
        return (r < 0) ? r+b : r;
    }

NAMESPACE_END(string)


/// random
NAMESPACE_BEGIN(random)
    /**
     * \brief Generate fast and reasonably good pseudorandom numbers using the
     * Tiny Encryption Algorithm (TEA) by David Wheeler and Roger Needham.
     *
     * For details, refer to "GPU Random Numbers via the Tiny Encryption Algorithm"
     * by Fahad Zafar, Marc Olano, and Aaron Curtis.
     *
     * \param v0
     *     First input value to be encrypted (could be the sample index)
     * \param v1
     *     Second input value to be encrypted (e.g. the requested random number dimension)
     * \param rounds
     *     How many rounds should be executed? The default for random number
     *     generation is 4.
     * \return
     *     A uniformly distributed 64-bit integer
     */
    uint64_t sampleTEA32(uint32_t v0, uint32_t v1, int rounds = 4);

    /**
     * \brief Generate pseudorandom permutation vector using the algorithm described in Pixar's
     * technical memo "Correlated Multi-Jittered Sampling":
     *
     *     https://graphics.pixar.com/library/MultiJitteredSampling/
     *
     *  Unlike \ref permute, this function supports permutation vectors of any length.
     *
     * \param i
     *     Input index to be mapped
     * \param l
     *     Length of the permutation vector or sample_count
     * \param p
     *     Seed value used as second input to the Tiny Encryption Algorithm. Can be used to
     *     generate different permutation vectors.
     * \return
     *     The index corresponding to the input index in the pseudorandom permutation vector.
     */
    uint32_t permute(uint32_t i, uint32_t l, uint32_t p);

NAMESPACE_END(random)


/// Measures associated with probability distributions
enum EMeasure {
    EUnknownMeasure = 0,
    ESolidAngle,
    EDiscrete
};

/// Compute a direction for the given coordinates in spherical coordinates
extern Vector3f sphericalDirection(float theta, float phi);

/// Compute a direction for the given coordinates in spherical coordinates
extern Point2f sphericalCoordinates(const Vector3f &dir);

/**
 * \brief Calculates the unpolarized fresnel reflection coefficient for a 
 * dielectric material. Handles incidence from either side (i.e.
 * \code cosThetaI<0 is allowed).
 *
 * \param cosThetaI
 *      Cosine of the angle between the normal and the incident ray
 * \param extIOR
 *      Refractive index of the side that contains the surface normal
 * \param intIOR
 *      Refractive index of the interior
 */
extern float fresnel(float cosThetaI, float extIOR, float intIOR);

/// refraction coefficient
extern Vector3f refract(const Vector3f &wi, const Vector3f &n, float eta);

/// reflection coefficient
extern Vector3f reflect(const Vector3f &wi, const Vector3f &n);

/**
 * \brief Return the global file resolver instance
 *
 * This class is used to locate resource files (e.g. mesh or
 * texture files) referenced by a scene being loaded
 */
extern filesystem::resolver *getFileResolver();

extern OIIO::TextureSystem *getTextureSystem();

template <typename... Args>
extern void LOG(const char *fmt, const Args &... args) {
    // fmt::print("{}[INFO]: {}", util::currentTime(), fmt::format(fmt, args...));
    std::cout << "[" << util::currentTime() << "][INFO]: " << fmt::format(fmt, args...) << std::endl;
}

inline float sqr(float x) {
    return x * x;
}

NAMESPACE_END(kazen)

