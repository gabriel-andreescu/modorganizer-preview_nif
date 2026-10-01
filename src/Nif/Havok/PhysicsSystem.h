#pragma once

#include "Nif/Havok/Packfile.h"
#include "Nif/Havok/ShapeGeometry.h"

#include <cstdint>
#include <span>

namespace Havok {
// The bodies of a Fallout 4 bhkPhysicsSystem. Each bhkNPCollisionObject selects one body by its ID.
class PhysicsSystem {
public:
    explicit PhysicsSystem(std::span<const char> a_bytes);

    [[nodiscard]] Body ReadBody(std::uint32_t a_bodyId) const;

private:
    Packfile _packfile;
    Packfile::Range _bodies;
};
} // namespace Havok
