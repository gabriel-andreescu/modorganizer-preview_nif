#pragma once

#include "Textures/FileLoad.h"
#include "Textures/SlotDescriptors.h"

#include <QString>
#include <QVector>

#include <cstddef>
#include <vector>

class OpenGLShape;
class TextureManager;

struct TextureUse {
    QString shapeName;
    std::size_t slot = 0;
    QString slotName;
    // The placeholder drawn in the slot because the texture did not load.
    TextureFallback fallback = TextureFallback::kNone;
    bool editorMarker = false;
};

struct TextureResult {
    QString path;
    QVector<TextureUse> uses;
    FileLoad file;
};

// One result per texture path, in the order the shapes first use them.
[[nodiscard]] QVector<TextureResult> CollectTextureResults(
    const std::vector<OpenGLShape>& a_shapes,
    const std::vector<OpenGLShape>& a_editorMarkerShapes,
    const TextureManager& a_textures
);
