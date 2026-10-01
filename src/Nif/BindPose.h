#pragma once

#include <nifly/Object3d.hpp>

#include <optional>

namespace nifly {
class NifFile;
class NiShape;
}

// Fits the rigid transform from a skinned shape's skin space to model space. Each bone's inverse skin-to-bone
// transform gives its position in skin space, its bone node gives the position in model space, and its total vertex
// weight sets how much it counts.
[[nodiscard]] std::optional<nifly::MatTransform> FitSkinToModel(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape);
