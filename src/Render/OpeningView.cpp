#include "Render/OpeningView.h"

namespace {
// How many times deeper than wide a mesh is before its side is shown instead of its front. A near-square footprint
// keeps the front, which is where buildings and furniture face.
constexpr float kSideRatio = 1.25F;
// Heads, hair and helmets reach about one and a half. Horses and dogs are over four.
constexpr float kSkinnedSideRatio = 2.0F;
// A mesh is seen from above when its height is under this share of the geometric mean of its width and depth.
// Blades, bows and roads are under a quarter. Vehicles and furniture are over a third.
constexpr float kFlatShare = 0.3F;

constexpr float kSideYaw = 90.0F;
constexpr float kTopPitch = 90.0F;
} // namespace

ViewAngles OpeningView(const nifly::Vector3& a_size, const bool a_skinned) {
    const auto sideRatio = a_skinned ? kSkinnedSideRatio : kSideRatio;
    const auto flat = a_size.z * a_size.z < kFlatShare * kFlatShare * a_size.x * a_size.y;
    return {
        .yaw = a_size.y > a_size.x * sideRatio ? kSideYaw : 0.0F,
        .pitch = flat ? kTopPitch : 0.0F,
    };
}
