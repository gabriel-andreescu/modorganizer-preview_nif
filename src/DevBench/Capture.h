#pragma once

#include "Preview/CameraSynchronizer.h"

#include <QSize>
#include <QString>

class NifPreviewWidget;

namespace DevBench {

struct CaptureRequest {
    PreviewPaneSide side = PreviewPaneSide::kLeft;
    QSize size {512, 512};
    // Resetting first makes the view independent of earlier camera movement.
    bool resetCamera = true;
    float yaw = 0.0F;
    float pitch = 0.0F;
    QString outputPath;
};

// Renders the pane offscreen and writes a PNG. Returns the written image size.
QSize CapturePane(const NifPreviewWidget& a_preview, const CaptureRequest& a_request);

}
