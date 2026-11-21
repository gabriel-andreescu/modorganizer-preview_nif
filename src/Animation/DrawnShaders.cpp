#include "Animation/DrawnShaders.h"
#include "Nif/ShapeVisibility.h"

#include <nifly/NifFile.hpp>

namespace Animation {

std::unordered_set<std::uint32_t> DrawnShaders(const nifly::NifFile& a_nifFile) {
    std::unordered_set<std::uint32_t> shaders;
    for (auto* const shape : a_nifFile.GetShapes()) {
        if (shape == nullptr) {
            continue;
        }
        const auto visibility = ClassifyShapeVisibility(a_nifFile, shape);
        if (visibility == ShapeVisibility::kHidden || visibility == ShapeVisibility::kInactiveSwitchChild) {
            continue;
        }
        if (auto* const shader = a_nifFile.GetShader(shape)) {
            shaders.insert(a_nifFile.GetBlockID(shader));
        }
    }
    return shaders;
}

} // namespace Animation
