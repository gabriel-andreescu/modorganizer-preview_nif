#include "CameraSynchronizer.h"

namespace {
float shortestAngleDelta(const float oldAngle, const float newAngle) {
    auto delta = newAngle - oldAngle;
    while (delta > 180.0f) {
        delta -= 360.0f;
    }
    while (delta < -180.0f) {
        delta += 360.0f;
    }

    return delta;
}
} // namespace

void CameraSynchronizer::updateSnapshot(const PreviewPaneSide side, const CameraState& state) {
    if (side == PreviewPaneSide::Left) {
        m_LeftCameraState = state;
        m_HasLeftCameraState = true;
        return;
    }

    m_RightCameraState = state;
    m_HasRightCameraState = true;
}

std::optional<CameraState> CameraSynchronizer::synchronizedTargetState(
    const PreviewPaneSide sourceSide,
    const CameraState& newSourceState,
    const CameraState& currentTargetState
) const {
    if (!hasSnapshot(sourceSide)) {
        return std::nullopt;
    }

    const auto oldSourceState = snapshot(sourceSide);
    auto targetState = currentTargetState;
    targetState.lookAt += newSourceState.lookAt - oldSourceState.lookAt;
    targetState.pitch += shortestAngleDelta(oldSourceState.pitch, newSourceState.pitch);
    targetState.yaw += shortestAngleDelta(oldSourceState.yaw, newSourceState.yaw);
    targetState.distance = oldSourceState.distance > 0.0f
                               ? targetState.distance * (newSourceState.distance / oldSourceState.distance)
                               : targetState.distance + (newSourceState.distance - oldSourceState.distance);

    return targetState;
}

bool CameraSynchronizer::hasSnapshot(const PreviewPaneSide side) const {
    return side == PreviewPaneSide::Left ? m_HasLeftCameraState : m_HasRightCameraState;
}

CameraState CameraSynchronizer::snapshot(const PreviewPaneSide side) const {
    return side == PreviewPaneSide::Left ? m_LeftCameraState : m_RightCameraState;
}
