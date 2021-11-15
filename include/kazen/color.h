#pragma once

#include <kazen/common.h>

NAMESPACE_BEGIN(kazen)

/// color3f
struct Color3f : public Eigen::Array3f {
public:
    using Base = Eigen::Array3f;

    Color3f(float value = 0.f) : Base(value, value, value) { }

    Color3f(float r, float g, float b) : Base(r, g, b) { }

    template <typename Derived> Color3f(const Eigen::ArrayBase<Derived>& p) : Base(p) { }

    template <typename Derived> Color3f &operator=(const Eigen::ArrayBase<Derived>& p) {
        this->Base::operator=(p);
        return *this;
    }

    float &r() { return x(); }
    float &g() { return y(); }
    float &b() { return z(); }
    const float &r() const { return x(); }
    const float &g() const { return y(); }
    const float &b() const { return z(); }

    Color3f clamp() const { return Color3f(std::max(r(), 0.0f),
        std::max(g(), 0.0f), std::max(b(), 0.0f)); }

    bool isValid() const;

    Color3f toLinearRGB() const;

    Color3f toSRGB() const;

    float getLuminance() const;

    std::string toString() const {
        return fmt::format("[{}, {}, {}]", coeff(0), coeff(1), coeff(2));
    }
};


/// color4f
struct Color4f : public Eigen::Array4f {
public:
    using Base = Eigen::Array4f;

    Color4f() : Base(0.0f, 0.0f, 0.0f, 0.0f) { }

    Color4f(const Color3f &c) : Base(c.r(), c.g(), c.b(), 1.0f) { }

    Color4f(float r, float g, float b, float w) : Base(r, g, b, w) { }

    template <typename Derived> Color4f(const Eigen::ArrayBase<Derived>& p) : Base(p) { }

    template <typename Derived> Color4f &operator=(const Eigen::ArrayBase<Derived>& p) {
        this->Base::operator=(p);
        return *this;
    }

    Color3f divideByFilterWeight() const {
        if (w() != 0)
            return head<3>() / w();
        else
            return Color3f(0.0f);
    }

    std::string toString() const {
        return fmt::format("[[{}, {}, {}, {}]", coeff(0), coeff(1), coeff(2), coeff(3));
    }
};

NAMESPACE_END(kazen)
