#include <kazen/common.h>

#include <kazen/color.h>
#include <kazen/transform.h>
#include <Eigen/Geometry>
#include <Eigen/LU>
#include <iomanip>

#if defined(__LINUX__)
#  if !defined(_GNU_SOURCE)
#    define _GNU_SOURCE
#  endif
#  include <dlfcn.h>
#  include <unistd.h>
#  include <limits.h>
#  include <sys/ioctl.h>
#elif defined(__OSX__)
#  include <sys/sysctl.h>
#  include <mach-o/dyld.h>
#  include <unistd.h>
#  include <sys/ioctl.h>
#elif defined(__WINDOWS__)
#  include <windows.h>
#endif


NAMESPACE_BEGIN(kazen)

NAMESPACE_BEGIN(util)
    // timeString
    std::string timeString(double time, bool precise)
    {
        if (std::isnan(time) || std::isinf(time))
            return "inf";

        std::string suffix = "ms";
        if (time > 1000) {
            time /= 1000; suffix = "s";
            if (time > 60) {
                time /= 60; suffix = "m";
                if (time > 60) {
                    time /= 60; suffix = "h";
                    if (time > 12) {
                        time /= 12; suffix = "d";
                    }
                }
            }
        }

        std::ostringstream os;
        os << std::setprecision(precise ? 4 : 1)
        << std::fixed << time << suffix;

        return os.str();
    }

    // terminalWidth
    int terminalWidth() {
        static int cachedWidth = -1;

        if (cachedWidth == -1) {
#if defined(__WINDOWS__)
            HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
            if (h != INVALID_HANDLE_VALUE && h != nullptr) {
                CONSOLE_SCREEN_BUFFER_INFO bufferInfo = {0};
                GetConsoleScreenBufferInfo(h, &bufferInfo);
                cachedWidth = bufferInfo.dwSize.X;
            }
#else
            struct winsize w;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0)
                cachedWidth = w.ws_col;
#endif
            if (cachedWidth == -1)
                cachedWidth = 80;
        }

        return cachedWidth;
    }

    // memString
    std::string memString(size_t size, bool precise) {
        const char *orders[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB"};
        float value = (float) size;
        
        int i=0;
        for (i = 0; i < 6 && value > 1024.f; ++i)
            value /= 1024.f;
        
        return fmt::format(precise ? "{:.0f}{}" : "{:.3f}{}", value, orders[i]);
    }

    /// copyright
    std::string copyright() {
        return fmt::format(
            "\n"
            "=======================================================\n"
            " nano-kazen: Physically Based Renderer                 \n"
            " Version Alpha {}.{}.{}a                               \n"
            " Copyright (C) {} {}. All rights reserved.             \n"
            "=======================================================\n",
            KAZEN_VERSION_MAJOR,
            KAZEN_VERSION_MINOR,
            KAZEN_VERSION_PATCH,
            KAZEN_YEAR,
            KAZEN_AUTHORS
        );
    }
NAMESPACE_END(util)


NAMESPACE_BEGIN(string)

    std::string indent(const std::string &string, int amount) {
        /* This could probably be done faster (it's not
        really speed-critical though) */
        std::istringstream iss(string);
        std::ostringstream oss;
        std::string spacer(amount, ' ');
        bool firstLine = true;
        for (std::string line; std::getline(iss, line); ) {
            if (!firstLine)
                oss << spacer;
            oss << line;
            if (!iss.eof())
                oss << endl;
            firstLine = false;
        }
        return oss.str();
    }

    bool endsWith(const std::string &value, const std::string &ending) {
        if (ending.size() > value.size())
            return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    std::string toLower(const std::string &value) {
        std::string result;
        result.resize(value.size());
        std::transform(value.begin(), value.end(), result.begin(), ::tolower);
        return result;
    }

    bool toBool(const std::string &str) {
        std::string value = toLower(str);
        if (value == "false")
            return false;
        else if (value == "true")
            return true;
        else
            throw Exception("Could not parse boolean value \"%s\"", str);
    }

    int toInt(const std::string &str) {
        char *end_ptr = nullptr;
        int result = (int) strtol(str.c_str(), &end_ptr, 10);
        if (*end_ptr != '\0')
            throw Exception("Could not parse integer value \"%s\"", str);
        return result;
    }

    unsigned int toUInt(const std::string &str) {
        char *end_ptr = nullptr;
        unsigned int result = (int) strtoul(str.c_str(), &end_ptr, 10);
        if (*end_ptr != '\0')
            throw Exception("Could not parse integer value \"%s\"", str);
        return result;
    }

    float toFloat(const std::string &str) {
        char *end_ptr = nullptr;
        float result = (float) strtof(str.c_str(), &end_ptr);
        if (*end_ptr != '\0')
            throw Exception("Could not parse floating point value \"%s\"", str);
        return result;
    }

    Eigen::Vector3f toVector3f(const std::string &str) {
        std::vector<std::string> tokens = tokenize(str);
        if (tokens.size() != 3)
            throw Exception("Expected 3 values");
        Eigen::Vector3f result;
        for (int i=0; i<3; ++i)
            result[i] = toFloat(tokens[i]);
        return result;
    }

    std::vector<std::string> tokenize(const std::string &string, const std::string &delim, bool includeEmpty) {
        std::string::size_type lastPos = 0, pos = string.find_first_of(delim, lastPos);
        std::vector<std::string> tokens;

        while (lastPos != std::string::npos) {
            if (pos != lastPos || includeEmpty)
                tokens.push_back(string.substr(lastPos, pos - lastPos));
            lastPos = pos;
            if (lastPos != std::string::npos) {
                lastPos += 1;
                pos = string.find_first_of(delim, lastPos);
            }
        }

        return tokens;
    }

NAMESPACE_END(string)


/// color
Color3f Color3f::toSRGB() const {
    Color3f result;

    for (int i=0; i<3; ++i) {
        float value = coeff(i);

        if (value <= 0.0031308f)
            result[i] = 12.92f * value;
        else
            result[i] = (1.0f + 0.055f)
                * std::pow(value, 1.0f/2.4f) -  0.055f;
    }

    return result;
}

Color3f Color3f::toLinearRGB() const {
    Color3f result;

    for (int i=0; i<3; ++i) {
        float value = coeff(i);

        if (value <= 0.04045f)
            result[i] = value * (1.0f / 12.92f);
        else
            result[i] = std::pow((value + 0.055f)
                * (1.0f / 1.055f), 2.4f);
    }

    return result;
}

bool Color3f::isValid() const {
    for (int i=0; i<3; ++i) {
        float value = coeff(i);
        if (value < 0 || !std::isfinite(value))
            return false;
    }
    return true;
}

float Color3f::getLuminance() const {
    return coeff(0) * 0.212671f + coeff(1) * 0.715160f + coeff(2) * 0.072169f;
}


/// transform
Transform::Transform(const Eigen::Matrix4f &trafo)
    : m_transform(trafo), m_inverse(trafo.inverse()) { }

std::string Transform::toString() const {
    std::ostringstream oss;
    oss << m_transform.format(Eigen::IOFormat(4, 0, ", ", ";\n", "", "", "[", "]"));
    return oss.str();
}

Transform Transform::operator*(const Transform &t) const {
    return Transform(m_transform * t.m_transform,
        t.m_inverse * m_inverse);
}

Vector3f sphericalDirection(float theta, float phi) {
    float sinTheta, cosTheta, sinPhi, cosPhi;

    sincosf(theta, &sinTheta, &cosTheta);
    sincosf(phi, &sinPhi, &cosPhi);

    return Vector3f(
        sinTheta * cosPhi,
        sinTheta * sinPhi,
        cosTheta
    );
}

Point2f sphericalCoordinates(const Vector3f &v) {
    Point2f result(
        std::acos(v.z()),
        std::atan2(v.y(), v.x())
    );
    if (result.y() < 0)
        result.y() += 2*M_PI;
    return result;
}

void coordinateSystem(const Vector3f &a, Vector3f &b, Vector3f &c) {
    if (std::abs(a.x()) > std::abs(a.y())) {
        float invLen = 1.0f / std::sqrt(a.x() * a.x() + a.z() * a.z());
        c = Vector3f(a.z() * invLen, 0.0f, -a.x() * invLen);
    } else {
        float invLen = 1.0f / std::sqrt(a.y() * a.y() + a.z() * a.z());
        c = Vector3f(0.0f, a.z() * invLen, -a.y() * invLen);
    }
    b = c.cross(a);
}

float fresnel(float cosThetaI, float extIOR, float intIOR) {
    float etaI = extIOR, etaT = intIOR;

    if (extIOR == intIOR)
        return 0.0f;

    /* Swap the indices of refraction if the interaction starts
       at the inside of the object */
    if (cosThetaI < 0.0f) {
        std::swap(etaI, etaT);
        cosThetaI = -cosThetaI;
    }

    /* Using Snell's law, calculate the squared sine of the
       angle between the normal and the transmitted ray */
    float eta = etaI / etaT,
          sinThetaTSqr = eta*eta * (1-cosThetaI*cosThetaI);

    if (sinThetaTSqr > 1.0f)
        return 1.0f;  /* Total internal reflection! */

    float cosThetaT = std::sqrt(1.0f - sinThetaTSqr);

    float Rs = (etaI * cosThetaI - etaT * cosThetaT)
             / (etaI * cosThetaI + etaT * cosThetaT);
    float Rp = (etaT * cosThetaI - etaI * cosThetaT)
             / (etaT * cosThetaI + etaI * cosThetaT);

    return (Rs * Rs + Rp * Rp) / 2.0f;
}

NAMESPACE_END(kazen)