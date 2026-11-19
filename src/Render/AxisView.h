#pragma once

#include "Render/OpeningView.h"

#include <cstdint>

// The views along a mesh's three axes. Each has an opposite: the back, the other side and the bottom.
enum class ViewAxis : std::uint8_t {
    kFront,
    kSide,
    kTop,
};

// The angles that show an axis view, or its opposite when the camera already shows that view. The top and bottom
// views keep a_topYaw, the yaw at which the mesh's longer side runs across the view.
[[nodiscard]] ViewAngles NextAxisView(ViewAxis a_axis, const ViewAngles& a_current, float a_topYaw);
