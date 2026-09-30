#include "Preview/CameraSynchronizer.h"

namespace {
float ShortestAngleDelta(const float a_oldAngle, const float a_newAngle) {
    auto delta = a_newAngle - a_oldAngle;
    while (delta > 180.0F) {
        delta -= 360.0F;
    }
    while (delta < -180.0F) {
        delta += 360.0F;
    }

    return delta;
}
} // namespace

void CameraSynchronizer::UpdateSnapshot(const PreviewPaneSide a_side, const CameraState& a_state) {
    if (a_side == PreviewPaneSide::kLeft) {
        _leftCameraState = a_state;
        _hasLeftCameraState = true;
        return;
    }

    _rightCameraState = a_state;
    _hasRightCameraState = true;
}

std::optional<CameraState> CameraSynchronizer::SynchronizedTargetState(
    const PreviewPaneSide a_sourceSide,
    const CameraState& a_newSourceState,
    const CameraState& a_currentTargetState
) const {
    if (!HasSnapshot(a_sourceSide)) {
        return std::nullopt;
    }

    const auto oldSourceState = Snapshot(a_sourceSide);
    auto targetState = a_currentTargetState;
    targetState.lookAt += a_newSourceState.lookAt - oldSourceState.lookAt;
    targetState.pitch += ShortestAngleDelta(oldSourceState.pitch, a_newSourceState.pitch);
    targetState.yaw += ShortestAngleDelta(oldSourceState.yaw, a_newSourceState.yaw);
    targetState.distance = oldSourceState.distance > 0.0F
                               ? targetState.distance * (a_newSourceState.distance / oldSourceState.distance)
                               : targetState.distance + (a_newSourceState.distance - oldSourceState.distance);

    return targetState;
}

bool CameraSynchronizer::HasSnapshot(const PreviewPaneSide a_side) const {
    return a_side == PreviewPaneSide::kLeft ? _hasLeftCameraState : _hasRightCameraState;
}

CameraState CameraSynchronizer::Snapshot(const PreviewPaneSide a_side) const {
    return a_side == PreviewPaneSide::kLeft ? _leftCameraState : _rightCameraState;
}
