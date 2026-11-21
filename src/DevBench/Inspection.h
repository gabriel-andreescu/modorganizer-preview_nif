#pragma once

#include <QJsonObject>

#include <optional>

class Camera;
class NifPreviewWidget;

namespace DevBench {

[[nodiscard]] QJsonObject PreviewSummary(int a_id, const NifPreviewWidget& a_preview);
// A time reports the animated shader values at that moment. Without one they are the values the view shows.
[[nodiscard]] QJsonObject PreviewDetails(int a_id, const NifPreviewWidget& a_preview, std::optional<float> a_time);
[[nodiscard]] QJsonObject CameraDetails(const Camera& a_camera);

}
