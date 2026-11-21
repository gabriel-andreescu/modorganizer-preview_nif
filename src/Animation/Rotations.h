#pragma once

#include <nifly/Animation.hpp>

namespace Animation {

// Whether the data has rotation keys of either kind: quaternions, or an angle about each axis.
[[nodiscard]] bool HasRotationKeys(const nifly::NiKeyframeData& a_data);

// Whether the data keys its rotation as an angle about each axis.
[[nodiscard]] bool HasAxisAngles(const nifly::NiKeyframeData& a_data);
// The rotation those angles give at a time.
[[nodiscard]] nifly::Quaternion AxisAnglesRotation(const nifly::NiKeyframeData& a_data, float a_time);

} // namespace Animation
