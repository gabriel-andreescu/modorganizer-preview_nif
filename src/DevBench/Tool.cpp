#include "DevBench/Tool.h"
#include "DevBench/CameraControl.h"
#include "DevBench/Capture.h"
#include "DevBench/Inspection.h"
#include "DevBench/Opener.h"
#include "Preview/Widget.h"

#include <DevBenchQt.h>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

#include <optional>
#include <stdexcept>

namespace {
QJsonObject Descriptor() {
    const QJsonObject properties {
        {
            "action",
            QJsonObject {{"type", "string"}, {"enum", QJsonArray {"list", "inspect", "open", "close", "camera"}}},
        },
        {"id", QJsonObject {{"type", "integer"}, {"minimum", 1}}},
        {"path", QJsonObject {{"type", "string"}}},
        {"pane", QJsonObject {{"type", "string"}, {"enum", QJsonArray {"left", "right"}}}},
        {"yaw", QJsonObject {{"type", "number"}}},
        {"pitch", QJsonObject {{"type", "number"}}},
        {"zoom", QJsonObject {{"type", "number"}, {"minimum", 0}}},
    };

    return {
        {
            "description",
            "Inspect live NIF previews: the NIF versions and texture sources each pane resolved, every shape's shader "
            "and program link state, and each texture slot's path, fallback and the file or archive that supplied it. "
            "list returns live previews with ids. inspect, close and camera take an id, or use the only visible preview. "
            "open previews a data path such as meshes/foo.nif in a standalone dialog. close closes the window showing "
            "the preview. camera rotates a pane by yaw and pitch "
            "degrees and multiplies its distance by zoom, as mouse input does. Use dialogs for the preview's own "
            "controls, and capture kind=preview_nif for a fixed-size render of a pane.",
        },
        {"readOnly", false},
        {
            "inputSchema",
            QJsonObject {
                {"type", "object"},
                {"properties", properties},
                {"required", QJsonArray {"action"}},
                {"additionalProperties", false},
            },
        },
    };
}

QJsonObject CaptureDescriptor() {
    const QJsonObject properties {
        {"id", QJsonObject {{"type", "integer"}, {"minimum", 1}}},
        {"pane", QJsonObject {{"type", "string"}, {"enum", QJsonArray {"left", "right"}}}},
        {"width", QJsonObject {{"type", "integer"}, {"minimum", 16}}},
        {"height", QJsonObject {{"type", "integer"}, {"minimum", 16}}},
        {"camera", QJsonObject {{"type", "string"}, {"enum", QJsonArray {"reset", "current"}}}},
        {"yaw", QJsonObject {{"type", "number"}}},
        {"pitch", QJsonObject {{"type", "number"}}},
    };
    return {
        {
            "description",
            "Renders a NIF preview pane offscreen without UI. id selects a preview from preview_nif list, or the only "
            "visible one. width and height default to 512. camera=reset (default) frames the mesh first, then yaw and "
            "pitch rotate it in degrees.",
        },
        {"inputSchema", QJsonObject {{"type", "object"}, {"properties", properties}}},
    };
}

std::optional<int> OptionalId(const QJsonObject& a_arguments) {
    return a_arguments.contains("id") ? std::optional(a_arguments["id"].toInt()) : std::nullopt;
}
}

namespace DevBench {

std::unique_ptr<Tool> Tool::Register(
    DevBenchAPI::IDevBenchInterface001& a_api,
    const PreviewNif& a_plugin,
    QWidget* a_mainWindow
) {
    auto tool = std::make_unique<Tool>(a_api, a_plugin, a_mainWindow);
    const auto descriptor = QJsonDocument(Descriptor()).toJson(QJsonDocument::Compact);
    if (!a_api.RegisterTool("preview_nif", descriptor.constData(), &Tool::Invoke, tool.get())) {
        qWarning("Dev Bench replaced an existing preview_nif tool");
    }
    const auto capture = QJsonDocument(CaptureDescriptor()).toJson(QJsonDocument::Compact);
    if (!a_api.RegisterToolExtension("capture", "preview_nif", capture.constData(), &Tool::InvokeCapture, tool.get())) {
        qWarning("Dev Bench replaced an existing preview_nif capture provider");
    }
    return tool;
}

Tool::Tool(DevBenchAPI::IDevBenchInterface001& a_api, const PreviewNif& a_plugin, QWidget* a_mainWindow)
    : _api {&a_api}
    , _plugin {&a_plugin}
    , _mainWindow {a_mainWindow} {}

void Tool::Invoke(void* a_ctx, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write) {
    auto* const tool = static_cast<Tool*>(a_ctx);
    DevBenchQt::RespondToTool(
        [tool](const QJsonObject& a_arguments) { return tool->Handle(a_arguments); },
        a_argsJson,
        a_sink,
        a_write
    );
}

void Tool::InvokeCapture(void* a_ctx, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write) {
    auto* const tool = static_cast<Tool*>(a_ctx);
    DevBenchQt::RespondToExtension(
        [tool](const QJsonObject& a_arguments) { return tool->Capture(a_arguments); },
        a_argsJson,
        a_sink,
        a_write
    );
}

QJsonObject Tool::Capture(const QJsonObject& a_arguments) {
    const auto entry = _previews.Find(OptionalId(a_arguments));
    const CaptureRequest request {
        .side = a_arguments["pane"].toString() == "right" ? PreviewPaneSide::kRight : PreviewPaneSide::kLeft,
        .size = {a_arguments["width"].toInt(512), a_arguments["height"].toInt(512)},
        .resetCamera = a_arguments["camera"].toString("reset") == "reset",
        .yaw = static_cast<float>(a_arguments["yaw"].toDouble()),
        .pitch = static_cast<float>(a_arguments["pitch"].toDouble()),
        .outputPath = a_arguments["outputPath"].toString(),
    };
    const auto size = CapturePane(*entry.widget, request);

    const QJsonObject ready {
        {"requestId", a_arguments["requestId"]},
        {"ok", true},
        {"uiExcluded", true},
        {"width", size.width()},
        {"height", size.height()},
    };
    _api->EmitEvent("capture.ready", QJsonDocument(ready).toJson(QJsonDocument::Compact).constData());
    return {{"id", entry.id}, {"written", request.outputPath}};
}

QJsonObject Tool::Handle(const QJsonObject& a_arguments) {
    const auto action = a_arguments["action"].toString();
    if (action == "list") {
        return List();
    }
    if (action == "inspect") {
        return Inspect(a_arguments);
    }
    if (action == "open") {
        return Open(a_arguments);
    }
    if (action == "close") {
        return Close(a_arguments);
    }
    if (action == "camera") {
        return ControlCamera(a_arguments);
    }
    throw std::runtime_error(QStringLiteral("Unknown action '%1'").arg(action).toStdString());
}

QJsonObject Tool::List() {
    QJsonArray previews;
    for (const auto& entry : _previews.Live()) {
        previews.append(PreviewSummary(entry.id, *entry.widget));
    }
    return {{"previews", previews}};
}

QJsonObject Tool::Inspect(const QJsonObject& a_arguments) {
    const auto entry = _previews.Find(OptionalId(a_arguments));
    return PreviewDetails(entry.id, *entry.widget);
}

QJsonObject Tool::Open(const QJsonObject& a_arguments) {
    const auto path = a_arguments["path"].toString();
    if (path.isEmpty()) {
        throw std::runtime_error("open requires a data path");
    }

    auto* const preview = OpenPreview(*_plugin, _mainWindow, path);
    return PreviewSummary(_previews.Track(preview), *preview);
}

QJsonObject Tool::Close(const QJsonObject& a_arguments) {
    const auto entry = _previews.Find(OptionalId(a_arguments));
    return {{"id", entry.id}, {"closed", entry.widget->window()->close()}};
}

QJsonObject Tool::ControlCamera(const QJsonObject& a_arguments) {
    const auto zoom = a_arguments["zoom"].toDouble(1.0);
    if (zoom <= 0.0) {
        throw std::runtime_error("zoom must be greater than 0");
    }

    const auto entry = _previews.Find(OptionalId(a_arguments));
    return MoveCamera(
        *entry.widget,
        {
            .side = a_arguments["pane"].toString() == "right" ? PreviewPaneSide::kRight : PreviewPaneSide::kLeft,
            .yaw = static_cast<float>(a_arguments["yaw"].toDouble()),
            .pitch = static_cast<float>(a_arguments["pitch"].toDouble()),
            .zoom = static_cast<float>(zoom),
        }
    );
}

}
