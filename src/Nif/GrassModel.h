#pragma once

#include <QString>

// Skyrim draws the models GRAS records use with its grass shader. The NIF has no marker for that, and vanilla and
// grass mods keep those models under meshes/landscape/grass/.
[[nodiscard]] bool IsGrassModelPath(const QString& a_dataPath);
