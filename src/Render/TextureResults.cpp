#include "Render/TextureResults.h"
#include "Render/Shape.h"
#include "Render/ShapeTextures.h"
#include "Textures/Manager.h"
#include "Textures/Source.h"

#include <QHash>

namespace {
void AppendShapeUses(
    QVector<TextureResult>& a_results,
    QHash<QString, qsizetype>& a_indices,
    const OpenGLShape& a_shape,
    const bool a_editorMarker,
    const TextureManager& a_textures
) {
    for (const auto& state : a_shape.Textures().SlotStates()) {
        const auto path = NormalizeTextureDataPath(state.requestedPath);
        if (path.isEmpty()) {
            continue;
        }

        const auto key = TextureDataPathKey(path);
        auto index = a_indices.value(key, -1);
        if (index < 0) {
            const auto* const file = a_textures.TextureFile(path);
            index = a_results.size();
            a_indices.insert(key, index);
            a_results.push_back({.path = path, .uses = {}, .file = file ? *file : FileLoad {.dataPath = path}});
        }

        a_results[index].uses.push_back({
            .shapeName = a_shape.Name(),
            .slot = state.descriptor.slot,
            .slotName = TextureSlotDisplayName(state.descriptor),
            .fallback = state.fallback,
            .editorMarker = a_editorMarker,
        });
    }
}
} // namespace

QVector<TextureResult> CollectTextureResults(
    const std::vector<OpenGLShape>& a_shapes,
    const std::vector<OpenGLShape>& a_editorMarkerShapes,
    const TextureManager& a_textures
) {
    QVector<TextureResult> results;
    QHash<QString, qsizetype> indices;
    for (const auto& shape : a_shapes) {
        AppendShapeUses(results, indices, shape, false, a_textures);
    }
    for (const auto& shape : a_editorMarkerShapes) {
        AppendShapeUses(results, indices, shape, true, a_textures);
    }
    return results;
}
