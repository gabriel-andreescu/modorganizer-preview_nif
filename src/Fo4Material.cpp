#include "Fo4Material.h"

#include <QDir>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace {
constexpr std::array<char, 4> BGSM = {'B', 'G', 'S', 'M'};
constexpr std::array<char, 4> BGEM = {'B', 'G', 'E', 'M'};
constexpr qsizetype TextureListOffset = 63;
constexpr qsizetype ShaderMaterialTextureCount = 9;
constexpr qsizetype EffectMaterialTextureCount = 5;

[[nodiscard]] bool startsWithMagic(const QByteArray& data, const std::array<char, 4>& magic) {
    return data.size()
           >= static_cast<qsizetype>(magic.size())
           && std::equal(magic.begin(), magic.end(), data.constData());
}

[[nodiscard]] std::uint32_t readUint32LE(const QByteArray& data, const qsizetype offset) {
    const auto* const bytes = reinterpret_cast<const unsigned char*>(data.constData() + offset);
    return static_cast<std::uint32_t>(bytes[0])
           | (static_cast<std::uint32_t>(bytes[1]) << 8)
           | (static_cast<std::uint32_t>(bytes[2]) << 16)
           | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

[[nodiscard]] bool readBethesdaString(const QByteArray& data, qsizetype& offset, QString& value) {
    if (offset < 0 || offset + 4 > data.size()) {
        return false;
    }

    const auto length = readUint32LE(data, offset);
    offset += 4;
    if (length == 0) {
        value.clear();
        return true;
    }

    if (std::cmp_greater(length, data.size() - offset)) {
        return false;
    }

    auto bytes = QByteArray(data.constData() + offset, static_cast<qsizetype>(length));
    offset += static_cast<qsizetype>(length);
    if (bytes.endsWith('\0')) {
        bytes.chop(1);
    }

    value = QDir::fromNativeSeparators(QString::fromUtf8(bytes)).trimmed();
    return true;
}
} // namespace

namespace Fo4Material {

QString normalizeMaterialDataPath(QString path) {
    path = QDir::fromNativeSeparators(path).trimmed();
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }
    if (path.startsWith("data/", Qt::CaseInsensitive)) {
        path.remove(0, 5);
    }

    const auto materialIndex = path.indexOf("materials/", 0, Qt::CaseInsensitive);
    if (materialIndex > 0) {
        path.remove(0, materialIndex);
    }

    return path;
}

Material read(const QByteArray& data) {
    const qsizetype textureCount = [&] {
        if (startsWithMagic(data, BGSM)) {
            return ShaderMaterialTextureCount;
        }
        if (startsWithMagic(data, BGEM)) {
            return EffectMaterialTextureCount;
        }
        return qsizetype {0};
    }();

    if (textureCount == 0 || data.size() < TextureListOffset) {
        return {};
    }

    Material material;
    qsizetype offset = TextureListOffset;
    for (qsizetype i = 0; i < textureCount; ++i) {
        QString texturePath;
        if (!readBethesdaString(data, offset, texturePath)) {
            return {};
        }
        material.textures.push_back(texturePath);
    }

    material.valid = true;
    return material;
}

}
