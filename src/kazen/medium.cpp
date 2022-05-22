#include <kazen/medium.h>

NAMESPACE_BEGIN(kazen)

class NonScatterMedium : public Medium {
public:
    NonScatterMedium(const PropertyList &propList) {
        // This method for calculating the absorption coefficient is borrowed 
        // from Burley's 2015 Siggraph Course Notes "Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering"
        // It's much more intutive to specify a color and a distance, then back-calculate the coefficient
        auto absorptionColor = propList.getColor("color", Color3f(0.5f));
        auto absorptionAtDistance = propList.getColor("atDistance", 1.f);

        m_absorptionCoefficient = -log(absorptionColor) / absorptionAtDistance;
    }

    float distance(const Ray3f &ray,  const Point2f &sample) const {
        return ray.maxt;
    }

    Color3f transmission(float distance) const {
        return exp(-m_absorptionCoefficient * distance);
    }

    std::string toString() const {
        return "NonScatterMedium";
    }

private:
    Color3f m_absorptionCoefficient;
};



KAZEN_REGISTER_CLASS(NonScatterMedium, "nonscatter");
NAMESPACE_END(kazen)