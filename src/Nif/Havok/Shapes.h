#pragma once

#include "Nif/Havok/ShapeGeometry.h"

#include <glm/mat4x4.hpp>

#include <cstdint>

namespace Havok {
class Packfile;

// Appends the geometry of the hknp shape object at a_shape and its children, moved by a_transform.
void ReadShape(
    const Packfile& a_packfile,
    std::uint32_t a_shape,
    const glm::mat4& a_transform,
    ShapeGeometry& a_geometry
);
} // namespace Havok
