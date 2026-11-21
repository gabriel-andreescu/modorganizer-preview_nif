#pragma once

#include <QString>

class NifPreviewWidget;
class PreviewNif;
class QWidget;

namespace DevBench {

// Hosts the preview in its own dialog, which lacks MO2's preview dialog chrome and origin switcher.
NifPreviewWidget* OpenPreview(const PreviewNif& a_plugin, QWidget* a_parent, const QString& a_dataPath);

}
