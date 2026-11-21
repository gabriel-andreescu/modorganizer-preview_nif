#include "Nif/GrassModel.h"

#include <QDir>

bool IsGrassModelPath(const QString& a_dataPath) {
    return QDir::fromNativeSeparators(a_dataPath).startsWith("meshes/landscape/grass/", Qt::CaseInsensitive);
}
