#include "Camera.h"

#include <cmath>

CameraState Camera::state() const
{
  return {m_LookAt, m_Pitch, m_Yaw, m_Distance};
}

void Camera::setState(const CameraState& state)
{
  m_LookAt    = state.lookAt;
  m_Pitch     = repeat(state.pitch, 0.0f, 360.0f);
  m_Yaw       = repeat(state.yaw, 0.0f, 360.0f);
  m_Distance  = std::max(10.0f, state.distance);
  m_NearPlane = qBound(10.0f, m_Distance - 5000.0f, 250.0f);
  m_FarPlane  = std::max(10000.0f, m_Distance * 2.0f);
  m_HasState  = true;
  cameraMoved();
}

void Camera::setDistance(const float distance)
{
  m_Distance  = std::max(10.0f, distance);
  m_NearPlane = qBound(10.0f, distance - 5000.0f, 250.0f);
  m_FarPlane  = std::max(10000.0f, distance * 2.0f);
  m_HasState  = true;
  cameraMoved();
}

void Camera::setLookAt(const QVector3D lookAt)
{
  m_LookAt   = lookAt;
  m_HasState = true;
  cameraMoved();
}

void Camera::pan(const QVector3D delta)
{
  m_LookAt += delta;
  m_HasState = true;
  cameraMoved();
}

void Camera::rotate(const float yaw, const float pitch)
{
  m_Yaw      = repeat(m_Yaw + yaw, 0.0f, 360.0f);
  m_Pitch    = repeat(m_Pitch + pitch, 0.0f, 360.0f);
  m_HasState = true;

  cameraMoved();
}

void Camera::zoomDistance(const float distance)
{
  setDistance(m_Distance + distance);

  cameraMoved();
}

void Camera::zoomFactor(const float factor)
{
  setDistance(m_Distance * factor);

  cameraMoved();
}

float Camera::repeat(const float value, const float min, const float max)
{
  return fmod(fmod(value, max - min) + (max - min), max - min) + min;
}
