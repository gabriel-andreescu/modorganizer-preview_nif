#include "Animation/Rotations.h"
#include "Animation/KeySampling.h"

#include <cmath>

namespace {
float AngleAt(const nifly::NiAnimationKeyGroup<float>& a_keys, const float a_time) {
    return a_keys.GetNumKeys() > 0 ? Animation::SampleKeys(a_keys, a_time) : 0.0F;
}
} // namespace

namespace Animation {

bool HasAxisAngles(const nifly::NiKeyframeData& a_data) {
    return a_data.rotationType == nifly::XYZ_ROTATION_KEY;
}

// The games build the rotation from the three angles this way, which turns about X first and Z last.
nifly::Quaternion AxisAnglesRotation(const nifly::NiKeyframeData& a_data, const float a_time) {
    const auto halfX = AngleAt(a_data.xRotations, a_time) * 0.5F;
    const auto halfY = AngleAt(a_data.yRotations, a_time) * 0.5F;
    const auto halfZ = AngleAt(a_data.zRotations, a_time) * 0.5F;
    const auto sinX = std::sin(halfX);
    const auto cosX = std::cos(halfX);
    const auto sinY = std::sin(halfY);
    const auto cosY = std::cos(halfY);
    const auto sinZ = std::sin(halfZ);
    const auto cosZ = std::cos(halfZ);
    return {
        (cosZ * cosY * cosX) + (sinZ * sinY * sinX),
        (cosZ * cosY * sinX) - (sinZ * sinY * cosX),
        (cosZ * sinY * cosX) + (sinZ * cosY * sinX),
        (sinZ * cosY * cosX) - (cosZ * sinY * sinX),
    };
}

bool HasRotationKeys(const nifly::NiKeyframeData& a_data) {
    if (HasAxisAngles(a_data)) {
        return a_data.xRotations.GetNumKeys() > 0
               || a_data.yRotations.GetNumKeys() > 0
               || a_data.zRotations.GetNumKeys() > 0;
    }
    return !a_data.quaternionKeys.empty();
}

} // namespace Animation
