#pragma once

#include <gli/gli.hpp>

#include <QString>

#include <cstddef>

namespace DdsTextures {

[[nodiscard]] gli::texture LoadIfValid(const char* a_data, std::size_t a_size);
[[nodiscard]] gli::texture LoadFileIfValid(const QString& a_path);

} // namespace DdsTextures
