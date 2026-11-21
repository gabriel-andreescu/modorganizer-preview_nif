#include "Animation/Curves.h"
#include "Animation/KeySampling.h"
#include "Animation/Quaternions.h"
#include "Animation/Rotations.h"

namespace {
// Files store the lowest float for a fixed value the interpolator doesn't have.
bool IsSet(const float a_value) {
    return a_value != nifly::NiFloatMin;
}

constexpr std::uint8_t kUnsetBool = 2;
} // namespace

namespace Animation {

float FloatCurve::At(const float a_time) const {
    return keys != nullptr ? SampleKeys(*keys, a_time) : value;
}

nifly::Vector3 PointCurve::At(const float a_time) const {
    return keys != nullptr ? SampleKeys(*keys, a_time) : value;
}

// The last key at or before the time decides, since a switch has nothing to blend.
bool BoolCurve::At(const float a_time) const {
    if (keys == nullptr) {
        return value;
    }

    auto shown = keys->GetKey(0).value;
    for (std::uint32_t i = 1; i < keys->GetNumKeys(); ++i) {
        const auto key = keys->GetKey(static_cast<int>(i));
        if (key.time > a_time) {
            break;
        }
        shown = key.value;
    }
    return shown != 0;
}

nifly::Matrix3 RotationCurve::At(const float a_time) const {
    if (quaternions) {
        return ToMatrix(quaternions->At(a_time));
    }
    return ToMatrix(axisAngles != nullptr ? AxisAnglesRotation(*axisAngles, a_time) : value);
}

void TransformCurve::Apply(nifly::MatTransform& a_transform, const float a_time) const {
    if (translation) {
        a_transform.translation = translation->At(a_time);
    }
    if (rotation) {
        a_transform.rotation = rotation->At(a_time);
    }
    if (scale) {
        a_transform.scale = scale->At(a_time);
    }
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

std::optional<PointCurve> PointCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* const a_from) {
    const auto* const interpolator = dynamic_cast<nifly::NiPoint3Interpolator*>(a_from);
    if (interpolator == nullptr) {
        return std::nullopt;
    }
    if (const auto* const data = a_header.GetBlock(interpolator->dataRef); data && data->data.GetNumKeys() > 0) {
        return PointCurve {.keys = &data->data};
    }
    const auto& value = interpolator->point3Value;
    return IsSet(value.x) && IsSet(value.y) && IsSet(value.z) ? std::optional(PointCurve {.value = value})
                                                              : std::nullopt;
}

std::optional<BoolCurve> BoolCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* const a_from) {
    const auto* const interpolator = dynamic_cast<nifly::NiBoolInterpolator*>(a_from);
    if (interpolator == nullptr) {
        return std::nullopt;
    }
    if (const auto* const data = a_header.GetBlock(interpolator->dataRef); data && data->data.GetNumKeys() > 0) {
        return BoolCurve {.keys = &data->data};
    }
    return interpolator->boolValue != kUnsetBool ? std::optional(BoolCurve {.value = interpolator->boolValue != 0})
                                                 : std::nullopt;
}

std::optional<TransformCurve> TransformCurveOf(const nifly::NiHeader& a_header, nifly::NiInterpolator* const a_from) {
    const auto* const interpolator = dynamic_cast<nifly::NiTransformInterpolator*>(a_from);
    if (interpolator == nullptr) {
        return std::nullopt;
    }

    // Keys win over the interpolator's fixed value for the same part, as in the games.
    const auto* const data = a_header.GetBlock(interpolator->dataRef);
    TransformCurve curve;
    if (data != nullptr && data->translations.GetNumKeys() > 0) {
        curve.translation = PointCurve {.keys = &data->translations};
    } else if (IsSet(interpolator->translation.x)) {
        curve.translation = PointCurve {.value = interpolator->translation};
    }
    if (data != nullptr && HasRotationKeys(*data)) {
        curve.rotation = HasAxisAngles(*data) ? RotationCurve {.axisAngles = data}
                                              : RotationCurve {.quaternions = QuaternionKeys(*data)};
    } else if (IsSet(interpolator->rotation.x)) {
        curve.rotation = RotationCurve {.value = interpolator->rotation};
    }
    if (data != nullptr && data->scales.GetNumKeys() > 0) {
        curve.scale = FloatCurve {.keys = &data->scales};
    } else if (IsSet(interpolator->scale)) {
        curve.scale = FloatCurve {.value = interpolator->scale};
    }

    if (!curve.translation && !curve.rotation && !curve.scale) {
        return std::nullopt;
    }
    return curve;
}

} // namespace Animation
