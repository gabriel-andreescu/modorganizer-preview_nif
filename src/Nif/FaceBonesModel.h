#pragma once

#include <QString>

// Fallout 4 loads a second NIF with this suffix next to headwear and head parts, for the bones that shape the face.
// Its controllers place those bones within the actor's skeleton, which the NIF by itself lacks, so playing them
// throws the mesh out of place.
[[nodiscard]] bool IsFaceBonesModelPath(const QString& a_dataPath);
