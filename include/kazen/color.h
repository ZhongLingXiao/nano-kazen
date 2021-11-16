#pragma once

#include <kazen/common.h>

NAMESPACE_BEGIN(kazen)

/**
 * \brief Represents a linear RGB color value
 */
struct Color3f : public Eigen::Array3f {
public:
    typedef Eigen::Array3f Base;

    /// Initialize the color vector with a uniform value
    Color3f(float value = 0.f) : Base(value, value, value) { }

    /// Initialize the color vector with specific per-channel values
    Color3f(float r, float g, float b) : Base(r, g, b) { }

    /// Construct a color vector from ArrayBase (needed to play nice with Eigen)
    template <typename Derived> Color3f(const Eigen::ArrayBase<Derived>& p) 
        : Base(p) { }

    /// Assign a color vector from ArrayBase (needed to play nice with Eigen)
    template <typename Derived> Color3f &operator=(const Eigen::ArrayBase<Derived>& p) {
        this->Base::operator=(p);
        return *this;
    }

    /// Return a reference to the red channel
    float &r() { return x(); }
    /// Return a reference to the red channel (const version)
    const float &r() const { return x(); }
    /// Return a reference to the green channel
    float &g() { return y(); }
    /// Return a reference to the green channel (const version)
    const float &g() const { return y(); }
    /// Return a reference to the blue channel
    float &b() { return z(); }
    /// Return a reference to the blue channel (const version)
    const float &b() const { return z(); }

    /// Clamp to the positive range
    Color3f clamp() const { return Color3f(std::max(r(), 0.0f),
        std::max(g(), 0.0f), std::max(b(), 0.0f)); }

    /// Check if the color vector contains a NaN/Inf/negative value
    bool isValid() const;

    /// Convert from sRGB to linear RGB
    Color3f toLinearRGB() const;

    /// Convert from linear RGB to sRGB
    Color3f toSRGB() const;

    /// Return the associated luminance
    float getLuminance() const;

    /// Return a human-readable string summary
    std::string toString() const {
        return fmt::format("[{}, {}, {}]", coeff(0), coeff(1), coeff(2));
    }
};

/**
 * \brief Represents a linear RGB color and a weight
 *
 * This is used by image reconstruction filter code
 */
struct Color4f : public Eigen::Array4f {
public:
    typedef Eigen::Array4f Base;

    /// Create an zero value
    Color4f() : Base(0.0f, 0.0f, 0.0f, 0.0f) { }

    /// Create from a 3-channel color
    Color4f(const Color3f &c) : Base(c.r(), c.g(), c.b(), 1.0f) { }

    /// Initialize the color vector with specific per-channel values
    Color4f(float r, float g, float b, float w) : Base(r, g, b, w) { }

    /// Construct a color vector from ArrayBase (needed to play nice with Eigen)
    template <typename Derived> Color4f(const Eigen::ArrayBase<Derived>& p) 
        : Base(p) { }

    /// Assign a color vector from ArrayBase (needed to play nice with Eigen)
    template <typename Derived> Color4f &operator=(const Eigen::ArrayBase<Derived>& p) {
        this->Base::operator=(p);
        return *this;
    }

    /// Divide by the filter weight and convert into a \ref Color3f value
    Color3f divideByFilterWeight() const {
        if (w() != 0)
            return head<3>() / w();
        else
            return Color3f(0.0f);
    }

    /// Return a human-readable string summary
    std::string toString() const {
        return fmt::format("[[{}, {}, {}, {}]", coeff(0), coeff(1), coeff(2), coeff(3));
    }
};

NAMESPACE_END(kazen)
