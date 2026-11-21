#pragma once

#include "Animation/ShaderValues.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace Animation {

// Every animated value of a NIF at one moment.
class Pose {
public:
    // The animated values of a shader block, or null when nothing animates it.
    [[nodiscard]] const ShaderValues* Shader(std::uint32_t a_block) const;
    // The values of a shader block to fill in.
    [[nodiscard]] ShaderValues& ShaderToAnimate(std::uint32_t a_block);

private:
    // A mesh animates a handful of shaders, so they are searched in order.
    std::vector<std::pair<std::uint32_t, ShaderValues>> _shaders;
};

} // namespace Animation
