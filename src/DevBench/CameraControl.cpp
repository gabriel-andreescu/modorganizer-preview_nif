#include "DevBench/CameraControl.h"
#include "DevBench/Inspection.h"
#include "Preview/Pane.h"
#include "Preview/Widget.h"
#include "Render/Camera.h"

#include <stdexcept>

namespace DevBench {

QJsonObject MoveCamera(const NifPreviewWidget& a_preview, const CameraMove& a_move) {
    if (a_move.side == PreviewPaneSide::kRight && !a_preview.IsSplitViewEnabled()) {
        throw std::runtime_error("The right pane is hidden while split preview is off");
    }

    const auto camera = a_preview.Pane(a_move.side)->SharedCamera();
    if (!camera) {
        throw std::runtime_error("The pane has no camera");
    }

    if (a_move.yaw != 0.0F || a_move.pitch != 0.0F) {
        camera->Rotate(a_move.yaw, a_move.pitch);
    }
    if (a_move.zoom != 1.0F) {
        camera->ZoomFactor(a_move.zoom);
    }

    QJsonObject result;
    if (const auto left = a_preview.Pane(PreviewPaneSide::kLeft)->SharedCamera()) {
        result.insert("left", CameraDetails(*left));
    }
    if (const auto right = a_preview.Pane(PreviewPaneSide::kRight)->SharedCamera();
        right && a_preview.IsSplitViewEnabled()) {
        result.insert("right", CameraDetails(*right));
    }
    return result;
}

}
