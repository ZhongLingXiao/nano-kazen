#include <kazen/warp.h>
#include <kazen/vector.h>
#include <kazen/frame.h>

NAMESPACE_BEGIN(kazen)

Point2f Warp::squareToUniformSquare(const Point2f &sample) {
    return sample;
}

float Warp::squareToUniformSquarePdf(const Point2f &sample) {
    return ((sample.array() >= 0).all() && (sample.array() <= 1).all()) ? 1.0f : 0.0f;
}

float Warp::intervalToTent(float sample) {
    float sign;

    if (sample < 0.5f) {
        sign = 1;
        sample *= 2;
    } else {
        sign = -1;
        sample = 2 * (sample - 0.5f);
    }

    return sign * (1 - std::sqrt(sample));
}


Point2f Warp::squareToTent(const Point2f &sample) {
    return Point2f(
        intervalToTent(sample.x()),
        intervalToTent(sample.y())
    );
}

float Warp::squareToTentPdf(const Point2f &p) {
    return (1.0f - std::abs(p.x())) * (1.0f - std::abs(p.y()));
}

Point2f Warp::squareToUniformDisk(const Point2f &sample) {
    float r = std::sqrt(sample.x());
    float sinPhi, cosPhi;
    math::sincosf(2.0f * M_PI * sample.y(), &sinPhi, &cosPhi);

    return Point2f(
        cosPhi * r,
        sinPhi * r
    );
}

float Warp::squareToUniformDiskPdf(const Point2f &p) {
    return INV_PI;
}

Vector3f Warp::squareToUniformSphere(const Point2f &sample) {
    float z = 1.0f - 2.0f * sample.y();
    float r = std::sqrt(1.0f - z*z);
    float sinPhi, cosPhi;
    math::sincosf(2.0f * M_PI * sample.x(), &sinPhi, &cosPhi);
    return Vector3f(r * cosPhi, r * sinPhi, z);
}

float Warp::squareToUniformSpherePdf(const Vector3f &v) {
    return INV_FOURPI;
}

Vector3f Warp::squareToUniformHemisphere(const Point2f &sample) {
    float z = sample.x();
    float tmp = std::sqrt(1.0f - z*z);

    float sinPhi, cosPhi;
    math::sincosf(2.0f * M_PI * sample.y(), &sinPhi, &cosPhi);

    return Vector3f(
        cosPhi * tmp, 
        sinPhi * tmp, 
        z);
}

float Warp::squareToUniformHemispherePdf(const Vector3f &v) {
    return INV_TWOPI;
}

Vector3f Warp::squareToCosineHemisphere(const Point2f &sample) {
    // squareToUniformDiskConcentric
    float r1 = 2.0f*sample.x() - 1.0f;
    float r2 = 2.0f*sample.y() - 1.0f;

    /* Modified concencric map code with less branching (by Dave Cline), see
       http://psgraphics.blogspot.ch/2011/01/improved-code-for-concentric-map.html */
    float phi, r;
    if (r1 == 0 && r2 == 0) {
        r = phi = 0;
    } else if (r1*r1 > r2*r2) {
        r = r1;
        phi = (M_PI/4.0f) * (r2/r1);
    } else {
        r = r2;
        phi = (M_PI/2.0f) - (r1/r2) * (M_PI/4.0f);
    }

    float cosPhi, sinPhi;
    math::sincosf(phi, &sinPhi, &cosPhi);

    // squareToCosineHemisphere
    Point2f p = Point2f(r * cosPhi, r * sinPhi);
    float z = std::sqrt(1.0f - p.x()*p.x() - p.y()*p.y());

    /* Guard against numerical imprecisions */
    if (z == 0)
        z = 1e-10f;

    return Vector3f(p.x(), p.y(), z);
}

float Warp::squareToCosineHemispherePdf(const Vector3f &v) {
    return INV_PI * Frame::cosTheta(v);
}


NAMESPACE_END(kazen)
