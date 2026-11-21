#pragma once

#include <gli/gli.hpp>

#include <cstddef>

namespace DdsTextures {

[[nodiscard]] gli::texture LoadIfValid(const char* a_data, std::size_t a_size);

} // namespace DdsTextures
