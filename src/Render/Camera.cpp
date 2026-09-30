#include "Render/Camera.h"

#include <cmath>

CameraState Camera::State() const {
    return {.lookAt = _lookAt, .pitch = _pitch, .yaw = _yaw, .distance = _distance};
}

void Camera::SetState(const CameraState& a_state) {
    _lookAt = a_state.lookAt;
    _pitch = Repeat(a_state.pitch, 0.0F, 360.0F);
    _yaw = Repeat(a_state.yaw, 0.0F, 360.0F);
    _distance = a_state.distance;
    _hasState = true;
    _framed = false;
    CameraMoved();
}

void Camera::Frame(const CameraState& a_state) {
    SetState(a_state);
    _framed = true;
}

void Camera::Refit(const float a_distance) {
    _distance = a_distance;
    CameraMoved();
}

void Camera::SetDistance(const float a_distance) {
    _distance = a_distance;
    _hasState = true;
    _framed = false;
    CameraMoved();
}

void Camera::SetLookAt(const QVector3D a_lookAt) {
    _lookAt = a_lookAt;
    _hasState = true;
    _framed = false;
    CameraMoved();
}

void Camera::Pan(const QVector3D a_delta) {
    _lookAt += a_delta;
    _hasState = true;
    _framed = false;
    CameraMoved();
}

void Camera::Rotate(const float a_yaw, const float a_pitch) {
    _yaw = Repeat(_yaw + a_yaw, 0.0F, 360.0F);
    _pitch = Repeat(_pitch + a_pitch, 0.0F, 360.0F);
    _hasState = true;

    CameraMoved();
}

void Camera::ZoomDistance(const float a_distance) {
    // A step that would reach the look-at point would flip the view through it.
    if (_distance + a_distance > 0.0F) {
        SetDistance(_distance + a_distance);
    }
}

void Camera::ZoomFactor(const float a_factor) {
    if (a_factor > 0.0F) {
        SetDistance(_distance * a_factor);
    }
}

float Camera::Repeat(const float a_value, const float a_min, const float a_max) {
    return fmod(fmod(a_value, a_max - a_min) + (a_max - a_min), a_max - a_min) + a_min;
}
