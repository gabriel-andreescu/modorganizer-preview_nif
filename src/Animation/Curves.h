#pragma once

#include "Animation/QuaternionKeys.h"

#include <nifly/Animation.hpp>

#include <optional>

namespace Animation {

// An interpolator's keys, or its fixed value when it has none.
struct FloatCurve {
    const nifly::NiAnimationKeyGroup<float>* keys = nullptr;
    float value = 0.0F;

    [[nodiscard]] float At(float a_time) const;
};

// A color or a position.
struct PointCurve {
    const nifly::NiAnimationKeyGroup<nifly::Vector3>* keys = nullptr;
    nifly::Vector3 value;

    [[nodiscard]] nifly::Vector3 At(float a_time) const;
};

struct BoolCurve {
    const nifly::NiAnimationKeyGroup<std::uint8_t>* keys = nullptr;
    bool value = false;

    [[nodiscard]] bool At(float a_time) const;
};

// Keys of one of the two kinds, or the fixed value without either.
struct RotationCurve {
    const nifly::NiKeyframeData* axisAngles = nullptr;
    std::optional<QuaternionKeys> quaternions;
    nifly::Quaternion value;

    [[nodiscard]] nifly::Matrix3 At(float a_time) const;
};

// The parts of an object's transform an interpolator animates. A part it leaves empty stays as the object has it.
struct TransformCurve {
    std::optional<PointCurve> translation;
    std::optional<RotationCurve> rotation;
    std::optional<FloatCurve> scale;

    void Apply(nifly::MatTransform& a_transform, float a_time) const;
};

// Empty for an interpolator that carries no value of its own, such as the blend interpolators a controller manager
// fills from its sequences.
[[nodiscard]] std::optional<FloatCurve> FloatCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* a_from);
[[nodiscard]] std::optional<PointCurve> PointCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* a_from);
[[nodiscard]] std::optional<BoolCurve> BoolCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* a_from);
[[nodiscard]] std::optional<TransformCurve> TransformCurveOf(
    const nifly::NiHeader& a_header,
    nifly::NiInterpolator* a_from
);

} // namespace Animation
