#pragma once

#include <nifly/Animation.hpp>

#include <optional>

namespace Animation {

// An interpolator's keys, or its fixed value when it has none.
struct FloatCurve {
    const nifly::NiAnimationKeyGroup<float>* keys = nullptr;
    float value = 0.0F;

    [[nodiscard]] float At(float a_time) const;
};

struct ColorCurve {
    const nifly::NiAnimationKeyGroup<nifly::Vector3>* keys = nullptr;
    nifly::Vector3 value;

    [[nodiscard]] nifly::Vector3 At(float a_time) const;
};

// Empty for an interpolator that carries no value of its own, such as the blend interpolators a controller manager
// fills from its sequences.
[[nodiscard]] std::optional<FloatCurve> FloatCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* a_from);
[[nodiscard]] std::optional<ColorCurve> ColorCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* a_from);

} // namespace Animation
