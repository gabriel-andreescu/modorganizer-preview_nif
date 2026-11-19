#include "DevBench/Inspection.h"
#include "Preview/Pane.h"
#include "Preview/Widget.h"
#include "Render/Camera.h"
#include "Render/NifWidget.h"
#include "Render/Shape.h"
#include "Render/ShapeTextures.h"
#include "Textures/Manager.h"
#include "Textures/SlotDescriptors.h"

#include <QJsonArray>
#include <QOpenGLShaderProgram>

#include <cstddef>

namespace {
QString ProviderKindName(const NifPreviewProviderKind a_kind) {
    switch (a_kind) {
        case NifPreviewProviderKind::kLooseFile: return QStringLiteral("loose");
        case NifPreviewProviderKind::kArchive:   return QStringLiteral("archive");
        case NifPreviewProviderKind::kInMemory:  return QStringLiteral("memory");
    }
    return {};
}

QString TextureSourceKindName(const TextureSourceProviderKind a_kind) {
    switch (a_kind) {
        case TextureSourceProviderKind::kAuto:     return QStringLiteral("auto");
        case TextureSourceProviderKind::kMod:      return QStringLiteral("mod");
        case TextureSourceProviderKind::kGameData: return QStringLiteral("gameData");
    }
    return {};
}

QString ShaderTypeName(const ShaderManager::ShaderType a_type) {
    switch (a_type) {
        case ShaderManager::kSKDefault:         return QStringLiteral("skDefault");
        case ShaderManager::kSKMSN:             return QStringLiteral("skModelSpaceNormals");
        case ShaderManager::kSKMultilayer:      return QStringLiteral("skMultilayer");
        case ShaderManager::kSKEffectShader:    return QStringLiteral("skEffect");
        case ShaderManager::kSKPBR:             return QStringLiteral("skPBR");
        case ShaderManager::kSKRefractionProxy: return QStringLiteral("skRefractionProxy");
        case ShaderManager::kCollisionWire:     return QStringLiteral("collisionWire");
        case ShaderManager::kFO4Default:        return QStringLiteral("fo4Default");
        case ShaderManager::kFO4EffectShader:   return QStringLiteral("fo4Effect");
        case ShaderManager::kNone:
        case ShaderManager::kShaderCount:       return QStringLiteral("none");
    }
    return {};
}

QString FallbackName(const TextureFallback a_fallback) {
    switch (a_fallback) {
        case TextureFallback::kError:      return QStringLiteral("error");
        case TextureFallback::kFlatNormal: return QStringLiteral("flatNormal");
        case TextureFallback::kBlack:      return QStringLiteral("black");
        case TextureFallback::kWhite:      return QStringLiteral("white");
        case TextureFallback::kNone:       return {};
    }
    return {};
}

void InsertIfNotEmpty(QJsonObject& a_object, const QString& a_key, const QString& a_value) {
    if (!a_value.isEmpty()) {
        a_object.insert(a_key, a_value);
    }
}

QJsonArray Vector(const QVector3D& a_vector) {
    return {a_vector.x(), a_vector.y(), a_vector.z()};
}

QJsonObject BoundsDetails(const nifly::BoundingSphere& a_bounds) {
    return {
        {"center", QJsonArray {a_bounds.center.x, a_bounds.center.y, a_bounds.center.z}},
        {"radius", a_bounds.radius},
    };
}

QString FileLoadStatusName(const FileLoadStatus a_status) {
    switch (a_status) {
        case FileLoadStatus::kMissing: return QStringLiteral("missing");
        case FileLoadStatus::kLoaded:  return QStringLiteral("loaded");
        case FileLoadStatus::kFailed:  return QStringLiteral("failed");
    }
    return {};
}

QJsonObject CopyDetails(const DataFileSource& a_copy) {
    QJsonObject details {
        {"kind", a_copy.kind == DataFileKind::kLoose ? QStringLiteral("loose") : QStringLiteral("archive")},
        {"path", a_copy.path},
    };
    InsertIfNotEmpty(details, "mod", a_copy.modName);
    InsertIfNotEmpty(details, "plugin", a_copy.plugin);
    if (a_copy.hidden) {
        details.insert("hidden", true);
    }
    return details;
}

void InsertFileLoad(QJsonObject& a_details, const FileLoad& a_file) {
    QJsonArray copies;
    for (const auto& copy : a_file.copies) {
        copies.append(CopyDetails(copy));
    }

    a_details.insert("dataPath", a_file.dataPath);
    a_details.insert("status", FileLoadStatusName(a_file.status));
    a_details.insert("copies", copies);
    if (a_file.usedCopy >= 0) {
        a_details.insert("usedCopy", static_cast<qint64>(a_file.usedCopy));
    }
    a_details.insert("picked", a_file.picked);
    a_details.insert("chosenSourceLacksFile", a_file.chosenSourceLacksFile);
}

QJsonObject TextureUseDetails(const TextureUse& a_use) {
    QJsonObject details {
        {"shape", a_use.shapeName},
        {"slot", static_cast<qint64>(a_use.slot)},
        {"slotName", a_use.slotName},
    };
    InsertIfNotEmpty(details, "fallback", FallbackName(a_use.fallback));
    if (a_use.editorMarker) {
        details.insert("editorMarker", true);
    }
    return details;
}

QJsonObject TextureResultDetails(const TextureResult& a_result) {
    QJsonArray uses;
    for (const auto& use : a_result.uses) {
        uses.append(TextureUseDetails(use));
    }

    QJsonObject details {
        {"path", a_result.path},
        {"uses", uses},
    };
    InsertFileLoad(details, a_result.file);
    return details;
}

QJsonObject ProviderDetails(const int a_index, const NifPreviewProvider& a_provider) {
    QJsonObject details {
        {"index", a_index},
        {"displayName", a_provider.displayName},
        {"kind", ProviderKindName(a_provider.kind)},
        {"virtualPath", a_provider.virtualPath},
    };
    InsertIfNotEmpty(details, "file", a_provider.absolutePath);
    InsertIfNotEmpty(details, "archive", a_provider.archivePath);
    InsertIfNotEmpty(details, "mod", a_provider.modName);
    details.insert("managed", a_provider.managed);
    if (a_provider.hidden) {
        details.insert("hidden", true);
    }
    return details;
}

QJsonObject TextureSourceDetails(const int a_index, const TextureSourceProvider& a_source) {
    QJsonObject details {
        {"index", a_index},
        {"displayName", a_source.displayName},
        {"kind", TextureSourceKindName(a_source.kind)},
        {"coveredTextures", static_cast<qint64>(a_source.coveredTextureCount)},
        {"totalTextures", static_cast<qint64>(a_source.totalTextureCount)},
    };
    InsertIfNotEmpty(details, "sourceName", a_source.sourceName);
    return details;
}

QJsonObject TextureReferenceDetails(const TextureReference& a_reference) {
    return {
        {"slot", a_reference.slot},
        {"slotName", a_reference.slotName},
        {"path", a_reference.path},
    };
}

QJsonObject SlotDetails(const TextureSlotState& a_state) {
    QJsonObject details {
        {"slot", static_cast<qint64>(a_state.descriptor.slot)},
        {"name", TextureSlotDisplayName(a_state.descriptor)},
        {"loaded", a_state.loaded},
    };
    InsertIfNotEmpty(details, "path", a_state.requestedPath);
    InsertIfNotEmpty(details, "fallback", FallbackName(a_state.fallback));
    return details;
}

QJsonObject ProgramDetails(const QOpenGLShaderProgram* a_program) {
    if (!a_program) {
        return {{"loaded", false}};
    }

    QJsonObject details {
        {"loaded", true},
        {"linked", a_program->isLinked()},
    };
    InsertIfNotEmpty(details, "log", a_program->log().trimmed());
    return details;
}

QJsonObject ShapeDetails(const OpenGLShape& a_shape, const NifWidget& a_view) {
    QJsonArray textures;
    for (const auto& state : a_shape.Textures().SlotStates()) {
        textures.append(SlotDetails(state));
    }

    QJsonObject details {
        {"name", a_shape.Name()},
        {"shader", ShaderTypeName(a_shape.ShaderType())},
        {"program", ProgramDetails(a_view.Shaders().LoadedProgram(a_shape.ShaderType()))},
        {"bounds", BoundsDetails(a_shape.Bounds())},
        {"refractionProxy", a_shape.IsRefractionProxy()},
        {"alphaPass", a_shape.UsesAlphaPass()},
        {"blendedPass", a_shape.UsesBlendedPass()},
        {"textures", textures},
    };

    if (const auto& materialPath = a_shape.MaterialPath(); !materialPath.isEmpty()) {
        QJsonObject material {{"path", materialPath}};
        if (const auto* const file = a_view.Textures().MaterialFile(materialPath)) {
            InsertFileLoad(material, *file);
        }
        details.insert("material", material);
    }
    return details;
}

QString PaneStatus(const PreviewPaneController& a_controller) {
    if (a_controller.CurrentNifFile()) {
        return QStringLiteral("loaded");
    }
    return a_controller.Providers().isEmpty() ? QStringLiteral("noProvider") : QStringLiteral("failed");
}

QJsonObject PaneDetails(const PreviewPaneSide a_side, const NifPreviewPane& a_pane) {
    const auto& controller = a_pane.Controller();

    QJsonArray providers;
    for (int i = 0; i < controller.Providers().size(); ++i) {
        providers.append(ProviderDetails(i, controller.Providers()[i]));
    }

    const auto& textureSourceSet = controller.TextureSources();
    QJsonArray textureSources;
    for (int i = 0; i < textureSourceSet.providers.size(); ++i) {
        textureSources.append(TextureSourceDetails(i, textureSourceSet.providers[i]));
    }

    QJsonArray textureReferences;
    for (const auto& reference : textureSourceSet.references) {
        textureReferences.append(TextureReferenceDetails(reference));
    }

    QJsonObject details {
        {"side", a_side == PreviewPaneSide::kLeft ? QStringLiteral("left") : QStringLiteral("right")},
        {"status", PaneStatus(controller)},
        {"collision", a_pane.ShowsCollision()},
        {"editorMarkers", a_pane.ShowsEditorMarkers()},
        {"providers", providers},
        {"currentProvider", controller.CurrentProviderIndex()},
        {"textureSources", textureSources},
        {"currentTextureSource", controller.CurrentTextureSourceIndex()},
        {"textureReferences", textureReferences},
        {"texturesOpen", a_pane.TexturesOpen()},
    };
    if (const auto nifFile = controller.CurrentNifFile()) {
        details.insert("stats", MakeNifStatsText(nifFile.get()));
    }
    if (const auto camera = a_pane.SharedCamera()) {
        details.insert("camera", DevBench::CameraDetails(*camera));
    }

    const auto* const view = a_pane.View();
    details.insert("rendered", view != nullptr && view->isValid());
    if (view != nullptr) {
        QJsonArray shapes;
        for (const auto& shape : view->Shapes()) {
            shapes.append(ShapeDetails(shape, *view));
        }
        for (const auto& shape : view->EditorMarkerShapes()) {
            auto markerDetails = ShapeDetails(shape, *view);
            markerDetails.insert("editorMarker", true);
            shapes.append(markerDetails);
        }
        details.insert("shapes", shapes);
        QJsonArray textures;
        for (const auto& result : view->TextureResults()) {
            textures.append(TextureResultDetails(result));
        }
        details.insert("textures", textures);
        details.insert("bounds", BoundsDetails(view->SceneBounds()));
        const auto size = view->SceneBox().Size();
        details.insert("size", QJsonArray {size.x, size.y, size.z});
    }
    return details;
}
}

namespace DevBench {

QJsonObject PreviewSummary(const int a_id, const NifPreviewWidget& a_preview) {
    return {
        {"id", a_id},
        {"virtualPath", a_preview.VirtualPath()},
        {"visible", a_preview.isVisible()},
        {"window", a_preview.window()->windowTitle()},
        {"split", a_preview.IsSplitViewEnabled()},
    };
}

QJsonObject PreviewDetails(const int a_id, const NifPreviewWidget& a_preview) {
    auto details = PreviewSummary(a_id, a_preview);
    details.insert("cameraSync", a_preview.IsCameraSyncEnabled());

    QJsonArray panes {PaneDetails(PreviewPaneSide::kLeft, *a_preview.Pane(PreviewPaneSide::kLeft))};
    if (a_preview.IsSplitViewEnabled()) {
        panes.append(PaneDetails(PreviewPaneSide::kRight, *a_preview.Pane(PreviewPaneSide::kRight)));
    }
    details.insert("panes", panes);
    return details;
}

QJsonObject CameraDetails(const Camera& a_camera) {
    return {
        {"framed", a_camera.IsFramed()},
        {"yaw", a_camera.Yaw()},
        {"pitch", a_camera.Pitch()},
        {"distance", a_camera.Distance()},
        {"lookAt", Vector(a_camera.LookAt())},
    };
}

}
