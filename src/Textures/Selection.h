#pragma once

#include "Textures/Source.h"

#include <QHash>
#include <QString>

// Which copy of each texture the preview reads.
struct TextureSelection {
    TextureSourceProvider source;
    // Copies picked for single textures, by texture key, each identified by its path. A pick overrides the source.
    QHash<QString, QString> pickedCopies;
};
