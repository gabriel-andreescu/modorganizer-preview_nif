#pragma once

#include "Nif/Havok/ShapeGeometry.h"

#include <glm/mat4x4.hpp>

#include <cstdint>

namespace Havok {
class Packfile;

// Appends the triangles, quads and custom primitives of the hknpCompressedMeshShape at a_shape, moved by a_transform.
void ReadCompressedMesh(
    const Packfile& a_packfile,
    std::uint32_t a_shape,
    const glm::mat4& a_transform,
    ShapeGeometry& a_geometry
);
} // namespace Havok
