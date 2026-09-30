#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace Fo4Material {

struct Textures {
    QString base;
    QString normal;
    QString specular;
    QString greyscale;
    QString environment;
    QString environmentMask;
    QString glow;
};

// Blend factors use NiAlphaProperty's numbering.
struct Alpha {
    bool blend = false;
    std::uint32_t sourceBlend = 6;
    std::uint32_t destinationBlend = 7;
    bool test = false;
    std::uint8_t testReference = 0;
};

struct Material {
    Textures textures;
    Alpha alpha;
    bool valid = false;
};

[[nodiscard]] QString NormalizeMaterialDataPath(QString a_path);
[[nodiscard]] Material Read(const QByteArray& a_data);

}
