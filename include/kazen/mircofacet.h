#pragma once
#include <kazen/common.h>
#include <kazen/vector.h>
#include <kazen/color.h>


NAMESPACE_BEGIN(kazen)
// This section refers to two papers by Eric Heitz:
// - 2014: Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs.
// - 2018: Sampling the GGX Distribution of Visible Normals.
class MicrofacetDistribution {
public:
    MicrofacetDistribution(float alphaU, float alphaV) 
        : m_alphaU(alphaU), m_alphaV(alphaV) {}

    // Evaluates the GGX-Smith VNDF (Dv), for the given view direction and half-vector and alpha.
    // NOTE: See 2018, equation #1. This is used when sampling the GGX-Smith VNDF, and should not be
    // used in place of the GGX NDF.
    float eval(Vector3f v, Vector3f h) const {
        return D(h) * G1(v, h) * std::abs(v.dot(h)) / std::abs(v.z());
    }

    // Compute the PDF for the GGX-Smith VNDF, with the specified view direction and microfacet normal.
    // NOTE: See 2018, appendix B.
    float pdf(Vector3f v, Vector3f h) const {
        // The PDF is simply the evaluation of the VNDF.
        return eval(v, h);
    }

    // Samples the GGX-Smith VNDF with the specified view direction, returning a (reflected) sample
    // direction and the PDF for that direction.
    // NOTE: See 2018, appendix A.    
    Vector3f sample(Vector3f v, Point2f sample2) const {
        // Section 3.2: Transform the view direction to the hemisphere configuration.
        Vector3f wh = Vector3f(m_alphaU * v.x(), m_alphaV * v.y(), v.z()).normalized();
        if (wh.z() < 0)
            wh = -wh;

        // Section 4.1: Orthonormal basis (with special case if cross product is zero).
        Vector3f T1 = (wh.z() < 0.99999f) ? Vector3f(0, 0, 1).cross(wh).normalized() : Vector3f(1, 0, 0);
        Vector3f T2 = wh.cross(T1).normalized();

        // Section 4.2: Parameterization of the projected area.
        float r   = std::sqrt(sample2[0]);
        float phi = 2.0f * M_PI * sample2[1];
        float t1  = r * cos(phi);
        float t2  = r * sin(phi);
        float s   = 0.5f * (1.0f + wh.z());
        t2        = (1.0f - s) * std::sqrt(1.0f - sqr(t1)) + s * t2;

        // Section 4.3: Reprojection onto hemisphere.
        Vector3f nh = t1 * T1 + t2 * T2 + std::sqrt(std::max(0.0f, 1.0f-sqr(t1)-sqr(t2))) * wh;

        // Section 3.4: Transform the normal back to the ellipsoid configuration.
        Vector3f H = Vector3f(m_alphaU * nh.x(), m_alphaV * nh.y(), std::max(1e-6f, nh.z())).normalized();

        return H;
    }

    // Evaluates the "lambda" expression used in Smith G1/G2 implementations, for the specified vector.
    // NOTE: See 2018, equation #2.
    float lambda(Vector3f w) const {
        float squared = (sqr(m_alphaU * w.x()) + sqr(m_alphaV * w.y())) / sqr(w.z());

        return (-1.0f + std::sqrt(1.0f + squared)) * 0.5f;
    }

    // Evaluates the Smith masking function (G1) for the specified view direction and alpha.
    // NOTE: See 2014, equation #43.
    float G1(Vector3f w, Vector3f m) const { 
        // Return zero if the view direction is below the half-vector, i.e. fully masked.
        if (w.dot(m) <= 0.0f)
            return 0.0f;
        return 1 / (1 + lambda(w)); 
    }

    // Evaluates the Smith masking and shadowing function (G2) for the specified view and light
    // directions and alpha.
    // NOTE: See 2014, equation #43.
    float G2(Vector3f wi, Vector3f wo) const {
        return 1 / (1 + lambda(wi) + lambda(wo));
    }

    // Evaluates the GGX NDF (D), for the given half-vector and alpha.
    // NOTE: See 2018, equation #1.
    float D(Vector3f wm) const {
        float ellipse = sqr(wm.x()/m_alphaU) + sqr(wm.y()/m_alphaV) + sqr(wm.z());
        
        return 1.0f / (M_PI * m_alphaU * m_alphaV * sqr(ellipse));
    }

    // Converts roughness to alpha for the GGX NDF and Smith geometry term.
    // NOTE: See "Revisiting Physically Based Shading at Imageworks" from SIGGRAPH 2017.
    static Vector2f roughnessToAlpha(float roughness, float anisotropy) {
        // Specify a minimum alpha value, as the GGX NDF does not support a zero alpha.
        static const float MIN_ALPHA = 0.001f;

        // Square the roughness value and combine with anisotropy to produce X and Y alpha values.
        float alpha = std::max(MIN_ALPHA, sqr(roughness));
        Vector2f alpha2 = Vector2f(alpha * (1.0f + anisotropy), alpha * (1.0f - anisotropy));

        return alpha2;
    }

private:
    float m_alphaU;
    float m_alphaV;
};

NAMESPACE_END(kazen)