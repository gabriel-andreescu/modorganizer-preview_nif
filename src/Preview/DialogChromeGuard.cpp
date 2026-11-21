#include "Preview/DialogChromeGuard.h"

#include <QStackedWidget>
#include <QStringList>
#include <QWidget>

PreviewDialogChromeGuard::~PreviewDialogChromeGuard() {
    Restore();
}

void PreviewDialogChromeGuard::HideFor(QWidget* a_previewWidget) {
    Capture(a_previewWidget);
    if (_hostChrome.isEmpty()) {
        return;
    }

    for (const auto& hostWidget : _hostChrome) {
        if (hostWidget.widget) {
            hostWidget.widget->hide();
        }
    }
}

void PreviewDialogChromeGuard::Restore() {
    for (const auto& hostWidget : _hostChrome) {
        if (hostWidget.widget) {
            hostWidget.widget->setVisible(hostWidget.wasVisible);
        }
    }
    _hostChrome.clear();
}

void PreviewDialogChromeGuard::Capture(QWidget* a_previewWidget) {
    if (!_hostChrome.isEmpty() || !a_previewWidget) {
        return;
    }

    const auto* const hostWindow = a_previewWidget->window();
    if (!hostWindow || hostWindow == a_previewWidget || hostWindow->objectName() != "PreviewDialog") {
        return;
    }

    const auto* const variantsStack = hostWindow->findChild<QStackedWidget*>("variantsStack");
    if (!variantsStack || !variantsStack->isAncestorOf(a_previewWidget)) {
        return;
    }

    // The file name stays. The mesh dropdown replaces the mod label and the buttons that step through the copies.
    const QStringList objectNames = {"modLabel", "previousButton", "nextButton"};
    for (const auto& objectName : objectNames) {
        if (auto* const widget = hostWindow->findChild<QWidget*>(objectName)) {
            _hostChrome.push_back({.widget = widget, .wasVisible = widget->isVisible()});
        }
    }
}
