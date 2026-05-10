#pragma once

#include "Camera.h"

#include <optional>

enum class PreviewPaneSide {
    Left,
    Right
};

class CameraSynchronizer final {
public:
    void updateSnapshot(PreviewPaneSide side, const CameraState& state);
    [[nodiscard]] std::optional<CameraState> synchronizedTargetState(
        PreviewPaneSide sourceSide,
        const CameraState& newSourceState,
        const CameraState& currentTargetState
    ) const;

private:
    [[nodiscard]] bool hasSnapshot(PreviewPaneSide side) const;
    [[nodiscard]] CameraState snapshot(PreviewPaneSide side) const;

    CameraState m_LeftCameraState;
    CameraState m_RightCameraState;
    bool m_HasLeftCameraState = false;
    bool m_HasRightCameraState = false;
};
