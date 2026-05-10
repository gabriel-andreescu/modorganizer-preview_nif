#include "PreviewDialogChromeGuard.h"

#include <QStackedWidget>
#include <QStringList>
#include <QWidget>

PreviewDialogChromeGuard::~PreviewDialogChromeGuard() {
    restore();
}

void PreviewDialogChromeGuard::hideFor(QWidget* previewWidget) {
    capture(previewWidget);
    if (m_HostChrome.isEmpty()) {
        return;
    }

    for (const auto& hostWidget : m_HostChrome) {
        if (hostWidget.widget) {
            hostWidget.widget->hide();
        }
    }
}

void PreviewDialogChromeGuard::restore() {
    for (const auto& hostWidget : m_HostChrome) {
        if (hostWidget.widget) {
            hostWidget.widget->setVisible(hostWidget.wasVisible);
        }
    }
    m_HostChrome.clear();
}

void PreviewDialogChromeGuard::capture(QWidget* previewWidget) {
    if (!m_HostChrome.isEmpty() || !previewWidget) {
        return;
    }

    auto* const hostWindow = previewWidget->window();
    if (!hostWindow || hostWindow == previewWidget || hostWindow->objectName() != "PreviewDialog") {
        return;
    }

    auto* const variantsStack = hostWindow->findChild<QStackedWidget*>("variantsStack");
    if (!variantsStack || !variantsStack->isAncestorOf(previewWidget)) {
        return;
    }

    const QStringList objectNames = {"nameLabel", "modLabel", "previousButton", "nextButton"};
    for (const auto& objectName : objectNames) {
        if (auto* const widget = hostWindow->findChild<QWidget*>(objectName)) {
            m_HostChrome.push_back({.widget = widget, .wasVisible = widget->isVisible()});
        }
    }
}
