#pragma once

#include "DevBench/Previews.h"

#include <DevBenchAPI.h>

#include <QJsonObject>
#include <QPointer>
#include <QWidget>

#include <memory>

class PreviewNif;

namespace DevBench {

class Tool final {
public:
    // The tool must outlive MO2's Dev Bench host, which keeps it as the handler context.
    [[nodiscard]] static std::unique_ptr<Tool> Register(
        DevBenchAPI::IDevBenchInterface001& a_api,
        const PreviewNif& a_plugin,
        QWidget* a_mainWindow
    );

    Tool(DevBenchAPI::IDevBenchInterface001& a_api, const PreviewNif& a_plugin, QWidget* a_mainWindow);

private:
    static void Invoke(void* a_ctx, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write);
    static void InvokeCapture(void* a_ctx, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write);

    [[nodiscard]] QJsonObject Handle(const QJsonObject& a_arguments);
    [[nodiscard]] QJsonObject List();
    [[nodiscard]] QJsonObject Inspect(const QJsonObject& a_arguments);
    [[nodiscard]] QJsonObject Open(const QJsonObject& a_arguments);
    [[nodiscard]] QJsonObject Close(const QJsonObject& a_arguments);
    [[nodiscard]] QJsonObject ControlCamera(const QJsonObject& a_arguments);
    [[nodiscard]] QJsonObject Browse(const QJsonObject& a_arguments);
    [[nodiscard]] QJsonObject CloseBrowser();
    [[nodiscard]] QJsonObject Capture(const QJsonObject& a_arguments);

    DevBenchAPI::IDevBenchInterface001* _api;
    const PreviewNif* _plugin;
    QPointer<QWidget> _mainWindow;
    Previews _previews;
};

}
