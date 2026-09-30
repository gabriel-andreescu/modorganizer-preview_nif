#pragma once

#include <QJsonObject>

class Camera;
class NifPreviewWidget;

namespace DevBench {

[[nodiscard]] QJsonObject PreviewSummary(int a_id, const NifPreviewWidget& a_preview);
[[nodiscard]] QJsonObject PreviewDetails(int a_id, const NifPreviewWidget& a_preview);
[[nodiscard]] QJsonObject CameraDetails(const Camera& a_camera);

}
