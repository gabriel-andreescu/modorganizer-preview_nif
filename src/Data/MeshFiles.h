#pragma once

#include <QString>

#include <set>

namespace MeshFiles {

// The file extensions the preview can show, lowercase and without the dot.
[[nodiscard]] const std::set<QString>& Extensions();
[[nodiscard]] bool IsMeshPath(const QString& a_path);

}
