#include "Animation/Quaternions.h"

#include <cmath>
#include <numbers>

namespace {
// Below this sine the games treat the angle as zero, where dividing by it would blow up.
constexpr float kSmallSine = 0.001F;

// The games' correction to a straight-line blend, which bends its pace to the arc's for rotations this far apart.
float ArcPace(const float a_along, const float a_cosine) {
    constexpr float kAttenuation = 0.8227969F;
    constexpr float kStrength = 0.5854922F;
    const auto spread = 1.0F - (a_cosine * kAttenuation);
    const auto bend = spread * spread * kStrength;
    const auto warp = [bend](const float a_part) {
        return ((((a_part + a_part) - 3.0F) * bend * a_part) + 1.0F + bend) * a_part;
    };
    return a_along > 0.5F ? 1.0F - warp(1.0F - a_along) : warp(a_along);
}
} // namespace

namespace Animation {

nifly::Matrix3 ToMatrix(const nifly::Quaternion& a_rotation) {
    const auto& turn = a_rotation;
    return {
        1.0F - (2.0F * ((turn.y * turn.y) + (turn.z * turn.z))),
        2.0F * ((turn.x * turn.y) - (turn.w * turn.z)),
        2.0F * ((turn.x * turn.z) + (turn.w * turn.y)),
        2.0F * ((turn.x * turn.y) + (turn.w * turn.z)),
        1.0F - (2.0F * ((turn.x * turn.x) + (turn.z * turn.z))),
        2.0F * ((turn.y * turn.z) - (turn.w * turn.x)),
        2.0F * ((turn.x * turn.z) - (turn.w * turn.y)),
        2.0F * ((turn.y * turn.z) + (turn.w * turn.x)),
        1.0F - (2.0F * ((turn.x * turn.x) + (turn.y * turn.y))),
    };
}

float Dot(const nifly::Quaternion& a_left, const nifly::Quaternion& a_right) {
    return (a_left.w * a_right.w) + (a_left.x * a_right.x) + (a_left.y * a_right.y) + (a_left.z * a_right.z);
}

nifly::Quaternion Negated(const nifly::Quaternion& a_rotation) {
    return {-a_rotation.w, -a_rotation.x, -a_rotation.y, -a_rotation.z};
}

nifly::Quaternion Inverse(const nifly::Quaternion& a_rotation) {
    return {a_rotation.w, -a_rotation.x, -a_rotation.y, -a_rotation.z};
}

nifly::Quaternion Product(const nifly::Quaternion& a_left, const nifly::Quaternion& a_right) {
    return {
        (a_left.w * a_right.w) - (a_left.x * a_right.x) - (a_left.y * a_right.y) - (a_left.z * a_right.z),
        (a_left.w * a_right.x) + (a_left.x * a_right.w) + (a_left.y * a_right.z) - (a_left.z * a_right.y),
        (a_left.w * a_right.y) + (a_left.y * a_right.w) + (a_left.z * a_right.x) - (a_left.x * a_right.z),
        (a_left.w * a_right.z) + (a_left.z * a_right.w) + (a_left.x * a_right.y) - (a_left.y * a_right.x),
    };
}

nifly::Vector3 Log(const nifly::Quaternion& a_rotation) {
    auto angle = 0.0F;
    if (a_rotation.w <= -1.0F) {
        angle = std::numbers::pi_v<float>;
    } else if (a_rotation.w < 1.0F) {
        angle = std::acos(a_rotation.w);
    }

    const auto sine = std::sin(angle);
    const auto scale = std::abs(sine) >= kSmallSine ? angle / sine : 1.0F;
    return {a_rotation.x * scale, a_rotation.y * scale, a_rotation.z * scale};
}

nifly::Quaternion Exp(const nifly::Vector3& a_turn) {
    const auto angle = std::sqrt((a_turn.x * a_turn.x) + (a_turn.y * a_turn.y) + (a_turn.z * a_turn.z));
    const auto sine = std::sin(angle);
    const auto scale = std::abs(sine) >= kSmallSine ? sine / angle : 1.0F;
    return {std::cos(angle), a_turn.x * scale, a_turn.y * scale, a_turn.z * scale};
}

nifly::Quaternion Slerp(const nifly::Quaternion& a_from, const nifly::Quaternion& a_to, const float a_along) {
    const auto pace = ArcPace(a_along, Dot(a_from, a_to));
    const nifly::Quaternion blend {
        a_from.w + ((a_to.w - a_from.w) * pace),
        a_from.x + ((a_to.x - a_from.x) * pace),
        a_from.y + ((a_to.y - a_from.y) * pace),
        a_from.z + ((a_to.z - a_from.z) * pace),
    };
    const auto scale = 1.0F / std::sqrt(Dot(blend, blend));
    return {blend.w * scale, blend.x * scale, blend.y * scale, blend.z * scale};
}

nifly::Quaternion Squad(
    const nifly::Quaternion& a_from,
    const nifly::Quaternion& a_leaving,
    const nifly::Quaternion& a_arriving,
    const nifly::Quaternion& a_to,
    const float a_along
) {
    return Slerp(
        Slerp(a_from, a_to, a_along),
        Slerp(a_leaving, a_arriving, a_along),
        (a_along + a_along) * (1.0F - a_along)
    );
}

} // namespace Animation
