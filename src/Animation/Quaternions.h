#pragma once

#include <nifly/BasicTypes.hpp>

// Quaternion arithmetic worked the way the games work it, so a rotation between keys lands where the game puts it.
namespace Animation {

[[nodiscard]] nifly::Matrix3 ToMatrix(const nifly::Quaternion& a_rotation);

[[nodiscard]] float Dot(const nifly::Quaternion& a_left, const nifly::Quaternion& a_right);
[[nodiscard]] nifly::Quaternion Negated(const nifly::Quaternion& a_rotation);
// The opposite turn of a rotation of length one.
[[nodiscard]] nifly::Quaternion Inverse(const nifly::Quaternion& a_rotation);
[[nodiscard]] nifly::Quaternion Product(const nifly::Quaternion& a_left, const nifly::Quaternion& a_right);

// A rotation as its axis scaled by half its angle, and back.
[[nodiscard]] nifly::Vector3 Log(const nifly::Quaternion& a_rotation);
[[nodiscard]] nifly::Quaternion Exp(const nifly::Vector3& a_turn);

// The games blend along the arc with a corrected straight line instead of sines, and they don't pick the shorter way
// round. Keys are turned to their neighbor's side once, when they are loaded.
[[nodiscard]] nifly::Quaternion Slerp(const nifly::Quaternion& a_from, const nifly::Quaternion& a_to, float a_along);

// The curve between two smooth keys, bent toward the control rotation each key has on that side.
[[nodiscard]] nifly::Quaternion Squad(
    const nifly::Quaternion& a_from,
    const nifly::Quaternion& a_leaving,
    const nifly::Quaternion& a_arriving,
    const nifly::Quaternion& a_to,
    float a_along
);

} // namespace Animation
