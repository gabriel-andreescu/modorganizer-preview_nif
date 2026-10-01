#pragma once

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>

namespace Havok {
[[nodiscard]] inline glm::vec3 Apply(const glm::mat4& a_transform, const glm::vec3& a_point) {
    return a_transform * glm::vec4(a_point, 1.0F);
}

// Spheres and capsules stay round under any scale, so they take the largest axis scale.
[[nodiscard]] inline float RadiusScale(const glm::mat4& a_transform) {
    return std::max({
        glm::length(glm::vec3(a_transform[0])),
        glm::length(glm::vec3(a_transform[1])),
        glm::length(glm::vec3(a_transform[2])),
    });
}
} // namespace Havok
