#pragma once

#include <gli/gli.hpp>

#include <QString>

#include <cstddef>

namespace DdsTextures {

[[nodiscard]] gli::texture loadIfValid(const char* data, std::size_t size);
[[nodiscard]] gli::texture loadFileIfValid(const QString& path);

} // namespace DdsTextures
