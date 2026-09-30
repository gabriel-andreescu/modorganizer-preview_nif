#pragma once

#include <gli/gli.hpp>

#include <QVector4D>

#include <memory>

class PreviewTexture;

namespace TextureUpload {

[[nodiscard]] std::unique_ptr<PreviewTexture> Upload(const gli::texture& a_texture);
[[nodiscard]] std::unique_ptr<PreviewTexture> MakeSolidColor(QVector4D a_color);

} // namespace TextureUpload
