#include "Animation/Pose.h"

#include <algorithm>

namespace Animation {

const ShaderValues* Pose::Shader(const std::uint32_t a_block) const {
    const auto found = std::ranges::find(_shaders, a_block, &std::pair<std::uint32_t, ShaderValues>::first);
    return found != _shaders.end() ? &found->second : nullptr;
}

ShaderValues& Pose::ShaderToAnimate(const std::uint32_t a_block) {
    const auto found = std::ranges::find(_shaders, a_block, &std::pair<std::uint32_t, ShaderValues>::first);
    return found != _shaders.end() ? found->second : _shaders.emplace_back(a_block, ShaderValues {}).second;
}

} // namespace Animation
