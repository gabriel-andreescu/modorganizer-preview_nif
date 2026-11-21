#pragma once

#include "Render/Camera.h"

#include <cstdint>
#include <optional>

enum class PreviewPaneSide : std::uint8_t {
    kLeft,
    kRight,
};

class CameraSynchronizer final {
public:
    void UpdateSnapshot(PreviewPaneSide a_side, const CameraState& a_state);
    [[nodiscard]] std::optional<CameraState> SynchronizedTargetState(
        PreviewPaneSide a_sourceSide,
        const CameraState& a_newSourceState,
        const CameraState& a_currentTargetState
    ) const;

private:
    [[nodiscard]] bool HasSnapshot(PreviewPaneSide a_side) const;
    [[nodiscard]] CameraState Snapshot(PreviewPaneSide a_side) const;

    CameraState _leftCameraState;
    CameraState _rightCameraState;
    bool _hasLeftCameraState = false;
    bool _hasRightCameraState = false;
};
