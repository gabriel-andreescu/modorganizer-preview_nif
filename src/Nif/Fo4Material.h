#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Fo4Material {

enum TextureIndex {
    kDiffuse = 0,
    kNormal = 1,
    kSpecular = 2,
    kGreyscale = 3,
    kEnvironment = 4,
    kGlowOrEnvironmentMask = 5,
};

struct Material {
    QStringList textures;
    bool valid = false;
};

[[nodiscard]] QString NormalizeMaterialDataPath(QString a_path);
[[nodiscard]] Material Read(const QByteArray& a_data);

}
