#include "DevBench/Capture.h"
#include "Preview/Pane.h"
#include "Preview/Widget.h"
#include "Render/Camera.h"
#include "Render/NifWidget.h"

#include <QImage>

#include <stdexcept>

namespace DevBench {

QSize CapturePane(const NifPreviewWidget& a_preview, const CaptureRequest& a_request) {
    if (a_request.side == PreviewPaneSide::kRight && !a_preview.IsSplitViewEnabled()) {
        throw std::runtime_error("The right pane is hidden while split preview is off");
    }
    auto* const view = a_preview.Pane(a_request.side)->View();
    if (view == nullptr) {
        throw std::runtime_error("The pane has no loaded NIF to render");
    }

    view->EnsureInitialized();
    if (a_request.resetCamera) {
        view->ResetCamera();
    }
    if (a_request.yaw != 0.0F || a_request.pitch != 0.0F) {
        view->SharedCamera()->Rotate(a_request.yaw, a_request.pitch);
    }

    const auto shownTime = view->AnimationTime();
    view->SetAnimationTime(a_request.time);
    const auto image = view->RenderImage(a_request.size);
    view->SetAnimationTime(shownTime);
    if (image.isNull() || !image.save(a_request.outputPath, "PNG")) {
        throw std::runtime_error(
            QStringLiteral("Could not write the preview capture to %1").arg(a_request.outputPath).toStdString()
        );
    }
    return image.size();
}

}
