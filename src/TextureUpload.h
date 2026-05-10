#pragma once

#include <gli/gli.hpp>

#include <QVector4D>

#include <memory>

class PreviewTexture;

namespace TextureUpload {

[[nodiscard]] std::unique_ptr<PreviewTexture> upload(const gli::texture& texture);
[[nodiscard]] std::unique_ptr<PreviewTexture> makeSolidColor(QVector4D color);

} // namespace TextureUpload
