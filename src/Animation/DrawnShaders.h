#pragma once

#include <cstdint>
#include <unordered_set>

namespace nifly {
class NifFile;
}

namespace Animation {

// The shader blocks of the shapes the preview draws. Animation on any other shader, such as a hidden shape's or a
// particle system's, shows nothing.
[[nodiscard]] std::unordered_set<std::uint32_t> DrawnShaders(const nifly::NifFile& a_nifFile);

} // namespace Animation
