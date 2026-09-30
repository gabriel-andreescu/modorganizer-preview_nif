#include "Render/Camera.h"

#include <algorithm>
#include <cmath>

CameraState Camera::State() const {
    return {.lookAt = _lookAt, .pitch = _pitch, .yaw = _yaw, .distance = _distance};
}

void Camera::SetState(const CameraState& a_state) {
    _lookAt = a_state.lookAt;
    _pitch = Repeat(a_state.pitch, 0.0F, 360.0F);
    _yaw = Repeat(a_state.yaw, 0.0F, 360.0F);
    _distance = std::max(10.0F, a_state.distance);
    _nearPlane = std::clamp(_distance - 5000.0F, 10.0F, 250.0F);
    _farPlane = std::max(10000.0F, _distance * 2.0F);
    _hasState = true;
    CameraMoved();
}

void Camera::SetDistance(const float a_distance) {
    _distance = std::max(10.0F, a_distance);
    _nearPlane = std::clamp(a_distance - 5000.0F, 10.0F, 250.0F);
    _farPlane = std::max(10000.0F, a_distance * 2.0F);
    _hasState = true;
    CameraMoved();
}

void Camera::SetLookAt(const QVector3D a_lookAt) {
    _lookAt = a_lookAt;
    _hasState = true;
    CameraMoved();
}

void Camera::Pan(const QVector3D a_delta) {
    _lookAt += a_delta;
    _hasState = true;
    CameraMoved();
}

void Camera::Rotate(const float a_yaw, const float a_pitch) {
    _yaw = Repeat(_yaw + a_yaw, 0.0F, 360.0F);
    _pitch = Repeat(_pitch + a_pitch, 0.0F, 360.0F);
    _hasState = true;

    CameraMoved();
}

void Camera::ZoomDistance(const float a_distance) {
    SetDistance(_distance + a_distance);

    CameraMoved();
}

void Camera::ZoomFactor(const float a_factor) {
    SetDistance(_distance * a_factor);

    CameraMoved();
}

float Camera::Repeat(const float a_value, const float a_min, const float a_max) {
    return fmod(fmod(a_value, a_max - a_min) + (a_max - a_min), a_max - a_min) + a_min;
}
