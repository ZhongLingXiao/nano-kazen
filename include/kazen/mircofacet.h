#pragma once
#include <kazen/common.h>
#include <kazen/vector.h>
#include <kazen/color.h>

NAMESPACE_BEGIN(kazen)
// =================================================================================================
// GGX-Smith BRDF
// =================================================================================================
//
// This section refers to two papers by Eric Heitz:
// - 2014: Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs.
// - 2018: Sampling the GGX Distribution of Visible Normals.

Color3f lerp(Color3f t, const float c1, const float c2) {
    return t * c1 + (1.f - t) * c2;
}


// Evaluates the Frensel equation for a conductor with the specified f0 value (i.e. reflectance at
// normal incidence), using the Schlick approximation.
Color3f evaluateSchlickFresnel(Color3f f0, float cosTheta) {
    return lerp(f0, 1.0f, pow(1.0f - cosTheta, 5.0f));
}

// Converts roughness to alpha for the GGX NDF and Smith geometry term.
// NOTE: See "Revisiting Physically Based Shading at Imageworks" from SIGGRAPH 2017.
Vector2f roughnessToAlpha(float roughness, float anisotropy) {
    // Specify a minimum alpha value, as the GGX NDF does not support a zero alpha.
    static const float MIN_ALPHA = 0.001f;

    // Square the roughness value and combine with anisotropy to produce X and Y alpha values.
    float alpha = std::max(MIN_ALPHA, sqr(roughness));
    Vector2f alpha2 = Vector2f(alpha * (1.0f + anisotropy), alpha * (1.0f - anisotropy));

    return alpha2;
}

// Evaluates the "lambda" expression used in Smith G1/G2 implementations, for the specified vector.
// NOTE: See 2018, equation #2.
float lambda(Vector3f vec, Vector2f alpha) {
    float squared = (sqr(alpha.x()) * sqr(vec.x()) + sqr(alpha.y()) * sqr(vec.y())) / sqr(vec.z());

    return (-1.0f + std::sqrt(1.0f + squared)) * 0.5f;
}

// Evaluates the Smith masking function (G1) for the specified view direction and alpha.
// NOTE: See 2014, equation #43.
float evaluateSmithG1(Vector3f V, Vector3f H, Vector2f alpha) {
    // Return zero if the view direction is below the half-vector, i.e. fully masked.
    if (V.dot(H) <= 0.0f)
        return 0.0f;

    return 1.0f / (1.0f + lambda(V, alpha));
}

// Evaluates the Smith masking and shadowing function (G2) for the specified view and light
// directions and alpha.
// NOTE: See 2014, equation #43.
float evaluateSmithG2(Vector3f V, Vector3f L, Vector3f H, Vector2f alpha) {
    // Return zero if the view or light direction is below the half-vector, i.e. fully masked and /
    // or shadowed.
    if (V.dot(H) <= 0.0f || L.dot(H) < 0.0f)
        return 0.0f;

    return 1.0f / (1.0f + lambda(V, alpha) + lambda(L, alpha));
}

// Evaluates the GGX NDF (D), for the given half-vector and alpha.
// NOTE: See 2018, equation #1.
float evaluateGGXNDF(Vector3f H, Vector2f alpha) {
    float ellipse = sqr(H.x()) / sqr(alpha.x()) + sqr(H.y()) / sqr(alpha.y()) + sqr(H.z());

    return 1.0f / (M_PI * alpha.x() * alpha.y() * sqr(ellipse));
}

// Evaluates the GGX-Smith VNDF (Dv), for the given view direction and half-vector and alpha.
// NOTE: See 2018, equation #1. This is used when sampling the GGX-Smith VNDF, and should not be
// used in place of the GGX NDF.
float evaluateGGXSmithVNDF(Vector3f V, Vector3f H, Vector2f alpha) {
    // Return zero if the view direction is below the half-vector, i.e. fully masked.
    float VDotH = V.dot(H);
    if (VDotH <= 0.0f)
        return 0.0f;

    // Evaluate the D and G1 components for the VNDF.
    float D  = evaluateGGXNDF(H, alpha);
    float G1 = evaluateSmithG1(V, H, alpha);

    return D * G1 * VDotH / V.z();
}

// Samples the GGX-Smith VNDF with the specified view direction, returning a (reflected) sample
// direction and the PDF for that direction.
// NOTE: See 2018, appendix A.
Vector3f sampleGGXSmithVNDF(Vector3f V, Vector2f alpha, const Point2f &random) {
    // Section 3.2: Transform the view direction to the hemisphere configuration.
    Vector3f Vh = Vector3f(alpha.x() * V.x(), alpha.y() * V.y(), V.z()).normalized();

    // Section 4.1: Orthonormal basis (with special case if cross product is zero).
    float lensq = Vh.x() * Vh.x() + Vh.y() * Vh.y();
    Vector3f T1 = lensq > 0.0f ? Vector3f(-Vh.y(), Vh.x(), 0.0f) / std::sqrt(lensq) : Vector3f(1.0f, 0.0f, 0.0f);
    Vector3f T2 = Vh.cross(T1).normalized();

    // Section 4.2: Parameterization of the projected area.
    float r   = std::sqrt(random.x());
    float phi = 2.0f * M_PI * random.y();
    float t1  = r * cos(phi);
    float t2  = r * sin(phi);
    float s   = 0.5f * (1.0f + Vh.z());
    t2        = (1.0f - s) * std::sqrt(1.0f - t1 * t1) + s * t2;

    // Section 4.3: Reprojection onto hemisphere.
    Vector3f Nh = t1 * T1 + t2 * T2 + std::sqrt(std::max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

    // Section 3.4: Transform the normal back to the ellipsoid configuration.
    Vector3f H = Vector3f(alpha.x() * Nh.x(), alpha.y() * Nh.y(), std::max(1e-6f, Nh.z())).normalized();

    return H;
}

// Compute the PDF for the GGX-Smith VNDF, with the specified view direction and microfacet normal.
// NOTE: See 2018, appendix B.
float computeGGXSmithPDF(Vector3f V, Vector3f H, Vector2f alpha) {
    // The PDF is simply the evaluation of the VNDF.
    return evaluateGGXSmithVNDF(V, H, alpha);
}

// Evaluates the GGX-Smith glossy BRDF, returning the BRDF value along with the Fresnel term used.
Color3f evaluateGGXSmith(Vector3f V, Vector3f L, float roughness, float anisotropy) {
    // Return black if the view and light directions are in opposite hemispheres.
    if (V.z() * L.z() < 0.0f)
        return Color3f(0.0f);

    // Convert roughness and anisotropy to alpha values.
    Vector2f alpha = roughnessToAlpha(roughness, anisotropy);

    // Compute the D (NDF), G (visibility), and F (Fresnel) terms, along with the microfacet BRDF
    // denominator.
    Vector3f H  = (V + L).normalized();
    float D     = evaluateGGXNDF(H, alpha);
    float G     = evaluateSmithG2(V, L, H, alpha);
    float denom = 4.0f * abs(V.z()) * abs(L.z());

    // Return the combined microfacet expression.
    return D * G / denom;
}


// Evaluates the GGX-Smith glossy BRDF, returning the BRDF value along with the Fresnel term used.
Color3f evaluateGGXSmithBRDF(Vector3f V, Vector3f L, Color3f f0, float roughness, 
    float anisotropy, Color3f &F) {
    // Return black if the view and light directions are in opposite hemispheres.
    if (V.z() * L.z() < 0.0f)
        return Color3f(0.0f);

    // Convert roughness and anisotropy to alpha values.
    Vector2f alpha = roughnessToAlpha(roughness, anisotropy);

    // Compute the D (NDF), G (visibility), and F (Fresnel) terms, along with the microfacet BRDF
    // denominator.
    Vector3f H  = (V + L).normalized();
    float D     = evaluateGGXNDF(H, alpha);
    float G     = evaluateSmithG2(V, L, H, alpha);
    F           = evaluateSchlickFresnel(f0, V.dot(H));
    float denom = 4.0f * abs(V.z()) * abs(L.z());

    // Return the combined microfacet expression.
    return D * G * F / denom;
}

// Samples and evaluates the GGX-Smith glossy BRDF, returning the BRDF value along with the sampled
// light direction and the PDF for that direction.
// NOTE: See 2018, appendix B.
Color3f sampleGGXSmithBRDF(Vector3f V, Color3f f0, float roughness, float anisotropy, 
    const Point2f &random, Vector3f &L, float &pdf) {
    // Convert roughness and anisotropy to alpha values.
    Vector2f alpha = roughnessToAlpha(roughness, anisotropy);

    // Sample the GGX-Smith VNDF to determine a sampled (light) direction. If the view direction is
    // in the lower hemisphere in tangent space, flip the view direction for sampling the VNDF, as
    // the VNDF expects the view direction to be in the upper hemisphere (with the normal). Then
    // flip the resulting microfacet normal.
    bool flip   = V.z() <= 0.0f;
    Vector3f H  = sampleGGXSmithVNDF(flip ? -V : V, alpha, random);
    H           = flip ? -H : H;

    // Reflect the view direction across the microfacet normal to get the sample direction.
    L = reflect(V, H);

    // Compute the PDF, divided by a factor from using a reflected vector. See 2018 equation #17.
    // NOTE: 2018 equation #19 shows that a number of terms can be cancelled out to produce a simple
    // estimator: just F * G2 / G1. Note that the PDF and cosine terms from the rendering equation
    // are cancelled out. However, doing this means the call sites must change: you will need
    // special handling for this BRDF compared to other BRDFs. There is no discernible performance
    // improvement by using the simpler estimator.
    float denom = 4.0f * V.dot(H);
    pdf         = computeGGXSmithPDF(V, H, alpha) / denom;

    // Evaluate the GGX-Smith BRDF with view direction and sampled light direction.
    Color3f F; // output not used
    return evaluateGGXSmithBRDF(V, L, f0, roughness, anisotropy, F);
}

NAMESPACE_END(kazen)