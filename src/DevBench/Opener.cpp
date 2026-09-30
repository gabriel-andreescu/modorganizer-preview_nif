#include "DevBench/Opener.h"
#include "Preview/Widget.h"
#include "PreviewNif.h"

#include <QDialog>
#include <QFileInfo>
#include <QVBoxLayout>

#include <stdexcept>

namespace {
constexpr QSize kDialogSize {900, 700};
}

namespace DevBench {

NifPreviewWidget* OpenPreview(const PreviewNif& a_plugin, QWidget* a_parent, const QString& a_dataPath) {
    auto* const preview = a_plugin.CreatePreview(a_dataPath, {});
    if (!preview) {
        throw std::runtime_error(
            QStringLiteral("No provider supplies a previewable NIF at '%1'").arg(a_dataPath).toStdString()
        );
    }

    auto* const dialog = new QDialog(a_parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QFileInfo(a_dataPath).fileName());
    auto* const layout = new QVBoxLayout(dialog);
    layout->addWidget(preview);
    dialog->resize(kDialogSize);
    dialog->show();
    return preview;
}

}
