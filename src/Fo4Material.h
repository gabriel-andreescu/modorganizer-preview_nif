#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Fo4Material {

enum TextureIndex {
    Diffuse = 0,
    Normal = 1,
    Specular = 2,
    Greyscale = 3,
    Environment = 4,
    GlowOrEnvironmentMask = 5,
};

struct Material {
    QStringList textures;
    bool valid = false;
};

[[nodiscard]] QString normalizeMaterialDataPath(QString path);
[[nodiscard]] Material read(const QByteArray& data);

}
