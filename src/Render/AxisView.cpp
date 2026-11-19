#include "Render/AxisView.h"

#include <cmath>

namespace {
constexpr float kHalfTurn = 180.0F;
constexpr float kQuarterTurn = 90.0F;
// Angles this close count as the same view, so a drag that snapped to it still flips to the opposite.
constexpr float kSameViewDegrees = 0.5F;

bool SameAngle(const float a_first, const float a_second) {
    const auto difference = std::fabs(std::remainder(a_first - a_second, 2.0F * kHalfTurn));
    return difference < kSameViewDegrees;
}

ViewAngles AxisView(const ViewAxis a_axis, const float a_topYaw) {
    switch (a_axis) {
        case ViewAxis::kFront: return {};
        case ViewAxis::kSide:  return {.yaw = kQuarterTurn};
        case ViewAxis::kTop:   return {.yaw = a_topYaw, .pitch = kQuarterTurn};
    }
    return {};
}

ViewAngles Opposite(const ViewAxis a_axis, const ViewAngles& a_view) {
    if (a_axis == ViewAxis::kTop) {
        return {.yaw = a_view.yaw, .pitch = a_view.pitch + kHalfTurn};
    }
    return {.yaw = a_view.yaw + kHalfTurn, .pitch = a_view.pitch};
}
} // namespace

ViewAngles NextAxisView(const ViewAxis a_axis, const ViewAngles& a_current, const float a_topYaw) {
    const auto view = AxisView(a_axis, a_topYaw);
    const auto shown = SameAngle(a_current.yaw, view.yaw) && SameAngle(a_current.pitch, view.pitch);
    return shown ? Opposite(a_axis, view) : view;
}
