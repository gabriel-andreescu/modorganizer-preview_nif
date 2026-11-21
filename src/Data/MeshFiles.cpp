#include "Data/MeshFiles.h"

namespace MeshFiles {

const std::set<QString>& Extensions() {
    static const std::set<QString> extensions {"bto", "btr", "nif"};
    return extensions;
}

bool IsMeshPath(const QString& a_path) {
    const auto dot = a_path.lastIndexOf('.');
    return dot >= 0 && Extensions().contains(a_path.sliced(dot + 1).toLower());
}

}
