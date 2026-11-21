#include "Animation/Curves.h"
#include "Animation/KeySampling.h"

namespace {
// Files store the lowest float for a fixed value the interpolator doesn't have.
bool IsSet(const float a_value) {
    return a_value != nifly::NiFloatMin;
}
} // namespace

namespace Animation {

float FloatCurve::At(const float a_time) const {
    return keys != nullptr ? SampleKeys(*keys, a_time) : value;
}

nifly::Vector3 ColorCurve::At(const float a_time) const {
    return keys != nullptr ? SampleKeys(*keys, a_time) : value;
}

std::optional<FloatCurve> FloatCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* const a_from) {
    const auto* const interpolator = dynamic_cast<nifly::NiFloatInterpolator*>(a_from);
    if (interpolator == nullptr) {
        return std::nullopt;
    }
    if (const auto* const data = a_header.GetBlock(interpolator->dataRef); data && data->data.GetNumKeys() > 0) {
        return FloatCurve {.keys = &data->data};
    }
    return IsSet(interpolator->floatValue) ? std::optional(FloatCurve {.value = interpolator->floatValue})
                                           : std::nullopt;
}

std::optional<ColorCurve> ColorCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* const a_from) {
    const auto* const interpolator = dynamic_cast<nifly::NiPoint3Interpolator*>(a_from);
    if (interpolator == nullptr) {
        return std::nullopt;
    }
    if (const auto* const data = a_header.GetBlock(interpolator->dataRef); data && data->data.GetNumKeys() > 0) {
        return ColorCurve {.keys = &data->data};
    }
    const auto& value = interpolator->point3Value;
    return IsSet(value.x) && IsSet(value.y) && IsSet(value.z) ? std::optional(ColorCurve {.value = value})
                                                              : std::nullopt;
}

} // namespace Animation
