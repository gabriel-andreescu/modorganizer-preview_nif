#include "Nif/Fo4Material.h"

#include <bodyslide/MaterialFile.h>

#include <QDir>

#include <cstddef>
#include <cstdint>
#include <span>
#include <spanstream>
#include <string>

namespace {
// Material strings keep their null terminator.
[[nodiscard]] QString TexturePath(const std::string& a_path) {
    return QDir::fromNativeSeparators(QString::fromUtf8(a_path.c_str())).trimmed();
}

[[nodiscard]] Fo4Material::Textures ShaderMaterialTextures(const MaterialFile& a_file) {
    // Fallout 4 uses the glow texture as the environment mask when the material disables glow.
    return {
        .base = TexturePath(a_file.diffuseTexture),
        .normal = TexturePath(a_file.normalTexture),
        .specular = TexturePath(a_file.smoothSpecTexture),
        .greyscale = TexturePath(a_file.greyscaleTexture),
        .environment = TexturePath(a_file.envmapTexture),
        .environmentMask = TexturePath(a_file.glowTexture),
        .glow = TexturePath(a_file.glowTexture),
    };
}

[[nodiscard]] Fo4Material::Textures EffectMaterialTextures(const MaterialFile& a_file) {
    return {
        .base = TexturePath(a_file.baseTexture),
        .normal = TexturePath(a_file.fxNormalTexture),
        .greyscale = TexturePath(a_file.grayscaleTexture),
        .environment = TexturePath(a_file.fxEnvmapTexture),
        .environmentMask = TexturePath(a_file.envmapMaskTexture),
    };
}

[[nodiscard]] Fo4Material::Alpha MaterialAlpha(MaterialFile& a_file) {
    std::uint8_t blend = 0;
    std::uint32_t sourceBlend = 0;
    std::uint32_t destinationBlend = 0;
    a_file.ConvertAlphaBlendMode(a_file.alphaBlendMode, blend, sourceBlend, destinationBlend);
    return {
        .blend = blend != 0,
        .sourceBlend = sourceBlend,
        .destinationBlend = destinationBlend,
        .test = a_file.alphaTest,
        .testReference = static_cast<std::uint8_t>(a_file.alphaTestRef),
    };
}
} // namespace

namespace Fo4Material {

QString NormalizeMaterialDataPath(QString a_path) {
    a_path = QDir::fromNativeSeparators(a_path).trimmed();
    while (a_path.startsWith('/')) {
        a_path.remove(0, 1);
    }
    if (a_path.startsWith("data/", Qt::CaseInsensitive)) {
        a_path.remove(0, 5);
    }

    const auto materialIndex = a_path.indexOf("materials/", 0, Qt::CaseInsensitive);
    if (materialIndex > 0) {
        a_path.remove(0, materialIndex);
    }

    return a_path;
}

Material Read(const QByteArray& a_data) {
    std::ispanstream stream(std::span(a_data.constData(), static_cast<std::size_t>(a_data.size())));
    MaterialFile file(stream);
    if (file.Failed()) {
        return {};
    }

    const auto textures = file.signature == MaterialFile::BGEM ? EffectMaterialTextures(file)
                                                               : ShaderMaterialTextures(file);
    return {.textures = textures, .alpha = MaterialAlpha(file), .valid = true};
}

}
