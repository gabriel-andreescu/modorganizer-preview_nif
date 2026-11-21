#pragma once

#include "Preview/CameraSynchronizer.h"

#include <QJsonObject>

class NifPreviewWidget;

namespace DevBench {

struct CameraMove {
    PreviewPaneSide side = PreviewPaneSide::kLeft;
    float yaw = 0.0F;
    float pitch = 0.0F;
    float zoom = 1.0F;
};

// Applies the move through the same camera calls as mouse input, so split-view sync follows it.
QJsonObject MoveCamera(const NifPreviewWidget& a_preview, const CameraMove& a_move);

}
