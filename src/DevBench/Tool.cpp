#include "DevBench/Tool.h"
#include "Browser/MeshBrowser.h"
#include "DevBench/CameraControl.h"
#include "DevBench/Capture.h"
#include "DevBench/Inspection.h"
#include "DevBench/Opener.h"
#include "Preview/Widget.h"
#include "PreviewNif.h"

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
            QJsonObject {
                {"type", "string"},
                {"enum", QJsonArray {"list", "inspect", "open", "close", "camera", "browse", "closeBrowser"}},
            },
        },
        {"id", QJsonObject {{"type", "integer"}, {"minimum", 1}}},
        {"path", QJsonObject {{"type", "string"}}},
        {"filter", QJsonObject {{"type", "string"}}},
        {"pane", QJsonObject {{"type", "string"}, {"enum", QJsonArray {"left", "right"}}}},
        {"yaw", QJsonObject {{"type", "number"}}},
        {"pitch", QJsonObject {{"type", "number"}}},
        {"zoom", QJsonObject {{"type", "number"}, {"minimum", 0}}},
        {"time", QJsonObject {{"type", "number"}, {"minimum", 0}}},
    };

    return {
        {
            "description",
            "Inspect live NIF previews: the NIF versions and texture sources each pane resolved, every shape's shader "
            "and program link state, each texture slot's path and fallback, and per texture the slots that use it, "
            "every copy in game order, the copy the preview read and its load status. "
            "list returns live previews with ids. inspect, close and camera take an id, or use the only visible preview. "
            "inspect reports whether each pane's mesh is animated and each shape's animated shader values, at time "
            "seconds into the animation when given and as shown otherwise. alphaPass and blendedPass always describe "
            "the shown moment. "
            "open previews a data path such as meshes/foo.nif in a standalone dialog. close closes the window showing "
            "the preview. camera rotates a pane by yaw and pitch "
            "degrees and multiplies its distance by zoom, as mouse input does. Use dialogs for the preview's own "
            "controls, and capture kind=preview_nif for a fixed-size render of a pane. "
            "browse opens the NIF Browser, or uses the open one, and returns whether its mesh list is ready, the mesh "
            "count, the filter text and the shown mesh with its preview id. filter sets the filter text. path selects "
            "and previews that mesh once the list is ready. closeBrowser closes it.",
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
        {"time", QJsonObject {{"type", "number"}, {"minimum", 0}}},
    };
    return {
        {
            "description",
            "Renders a NIF preview pane offscreen without UI. id selects a preview from preview_nif list, or the only "
            "visible one. width and height default to 512. camera=reset (default) frames the mesh first, then yaw and "
            "pitch rotate it in degrees. time renders that many seconds into the mesh's animation. Without it the "
            "mesh renders at rest.",
        },
        {"inputSchema", QJsonObject {{"type", "object"}, {"properties", properties}}},
    };
}

std::optional<int> OptionalId(const QJsonObject& a_arguments) {
    return a_arguments.contains("id") ? std::optional(a_arguments["id"].toInt()) : std::nullopt;
}

std::optional<float> OptionalTime(const QJsonObject& a_arguments) {
    return a_arguments.contains("time") ? std::optional(static_cast<float>(a_arguments["time"].toDouble()))
                                        : std::nullopt;
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
        .time = OptionalTime(a_arguments),
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
    if (action == "browse") {
        return Browse(a_arguments);
    }
    if (action == "closeBrowser") {
        return CloseBrowser();
    }
    throw std::runtime_error(QStringLiteral("Unknown action '%1'").arg(action).toStdString());
}

QJsonObject Tool::Browse(const QJsonObject& a_arguments) {
    auto* const browser = _plugin->ShowBrowser(_mainWindow);
    if (a_arguments.contains("filter")) {
        browser->SetFilterText(a_arguments["filter"].toString());
    }
    if (const auto path = a_arguments["path"].toString(); !path.isEmpty()) {
        if (!browser->IsReady()) {
            throw std::runtime_error("The NIF Browser is still reading archives");
        }
        if (!browser->ShowMesh(path)) {
            throw std::runtime_error(
                QStringLiteral("The NIF Browser lists no mesh at '%1' with its current filter").arg(path).toStdString()
            );
        }
    }

    QJsonObject state {
        {"ready", browser->IsReady()},
        {"meshes", static_cast<qint64>(browser->MeshCount())},
        {"filter", browser->FilterText()},
        {"shown", browser->ShownPath()},
    };
    if (auto* const preview = browser->Preview()) {
        state.insert("preview", _previews.Track(preview));
    }
    return state;
}

QJsonObject Tool::CloseBrowser() {
    auto* const browser = _plugin->Browser();
    return {{"closed", browser != nullptr && browser->close()}};
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
    return PreviewDetails(entry.id, *entry.widget, OptionalTime(a_arguments));
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
