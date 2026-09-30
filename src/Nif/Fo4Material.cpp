#include "Nif/Fo4Material.h"

#include <QByteArrayView>
#include <QDir>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace {
constexpr std::array<char, 4> kBGSM = {'B', 'G', 'S', 'M'};
constexpr std::array<char, 4> kBGEM = {'B', 'G', 'E', 'M'};
constexpr qsizetype kTextureListOffset = 63;
constexpr qsizetype kShaderMaterialTextureCount = 9;
constexpr qsizetype kEffectMaterialTextureCount = 5;

[[nodiscard]] bool StartsWithMagic(const QByteArray& a_data, const std::array<char, 4>& a_magic) {
    return a_data.size() >= static_cast<qsizetype>(a_magic.size())
           && std::equal(a_magic.begin(), a_magic.end(), a_data.constData());
}

[[nodiscard]] std::uint32_t ReadUint32LE(const QByteArray& a_data, const qsizetype a_offset) {
    return qFromLittleEndian<std::uint32_t>(QByteArrayView(a_data).sliced(a_offset, 4).data());
}

[[nodiscard]] bool ReadBethesdaString(const QByteArray& a_data, qsizetype& a_offset, QString& a_value) {
    if (a_offset < 0 || a_offset + 4 > a_data.size()) {
        return false;
    }

    const auto length = ReadUint32LE(a_data, a_offset);
    a_offset += 4;
    if (length == 0) {
        a_value.clear();
        return true;
    }

    if (std::cmp_greater(length, a_data.size() - a_offset)) {
        return false;
    }

    auto bytes = a_data.sliced(a_offset, static_cast<qsizetype>(length));
    a_offset += static_cast<qsizetype>(length);
    if (bytes.endsWith('\0')) {
        bytes.chop(1);
    }

    a_value = QDir::fromNativeSeparators(QString::fromUtf8(bytes)).trimmed();
    return true;
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
    const qsizetype textureCount = [&] {
        if (StartsWithMagic(a_data, kBGSM)) {
            return kShaderMaterialTextureCount;
        }
        if (StartsWithMagic(a_data, kBGEM)) {
            return kEffectMaterialTextureCount;
        }
        return qsizetype {0};
    }();

    if (textureCount == 0 || a_data.size() < kTextureListOffset) {
        return {};
    }

    Material material;
    qsizetype offset = kTextureListOffset;
    for (qsizetype i = 0; i < textureCount; ++i) {
        QString texturePath;
        if (!ReadBethesdaString(a_data, offset, texturePath)) {
            return {};
        }
        material.textures.push_back(texturePath);
    }

    material.valid = true;
    return material;
}

}
