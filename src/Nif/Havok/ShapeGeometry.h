#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace Havok {
struct Sphere {
    glm::vec3 center {};
    float radius = 0.0F;
};

struct Capsule {
    glm::vec3 start {};
    glm::vec3 end {};
    float radius = 0.0F;
};

// A body's collision shapes in its local space and Havok units.
struct ShapeGeometry {
    // Closed outlines of faces, triangles and quads.
    std::vector<std::vector<glm::vec3>> polygons;
    // Point clouds whose convex hull is the shape.
    std::vector<std::vector<glm::vec3>> hulls;
    std::vector<Sphere> spheres;
    std::vector<Capsule> capsules;
};

struct Body {
    std::uint32_t collisionFilterInfo = 0;
    ShapeGeometry geometry;
};
} // namespace Havok
