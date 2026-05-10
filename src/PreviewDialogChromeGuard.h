#pragma once

#include <QVector>

#include <QPointer>

class QWidget;

class PreviewDialogChromeGuard final {
public:
    ~PreviewDialogChromeGuard();

    void hideFor(QWidget* previewWidget);
    void restore();

private:
    struct HostChromeWidget {
        QPointer<QWidget> widget;
        bool wasVisible = false;
    };

    void capture(QWidget* previewWidget);

    QVector<HostChromeWidget> m_HostChrome;
};
