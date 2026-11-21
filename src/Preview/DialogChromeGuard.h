#pragma once

#include <QVector>

#include <QPointer>

class QWidget;

class PreviewDialogChromeGuard final {
public:
    PreviewDialogChromeGuard() = default;
    ~PreviewDialogChromeGuard();
    PreviewDialogChromeGuard(const PreviewDialogChromeGuard&) = delete;
    PreviewDialogChromeGuard(PreviewDialogChromeGuard&&) = delete;
    PreviewDialogChromeGuard& operator=(const PreviewDialogChromeGuard&) = delete;
    PreviewDialogChromeGuard& operator=(PreviewDialogChromeGuard&&) = delete;

    void HideFor(QWidget* a_previewWidget);
    void Restore();

private:
    struct HostChromeWidget {
        QPointer<QWidget> widget;
        bool wasVisible = false;
    };

    void Capture(QWidget* a_previewWidget);

    QVector<HostChromeWidget> _hostChrome;
};
