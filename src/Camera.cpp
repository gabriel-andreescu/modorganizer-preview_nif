#include "Camera.h"

#include <cmath>

void Camera::setDistance(const float distance)
{
  m_Distance  = std::max(10.0f, distance);
  m_NearPlane = qBound(10.0f, distance - 100.0f, 250.0f);
  m_FarPlane  = std::max(10000.0f, distance * 2.0f);
  cameraMoved();
}

void Camera::setLookAt(const QVector3D lookAt)
{
  m_LookAt = lookAt;
  cameraMoved();
}

void Camera::pan(const QVector3D delta)
{
  m_LookAt += delta;
  cameraMoved();
}

void Camera::rotate(const float yaw, const float pitch)
{
  m_Yaw   = repeat(m_Yaw + yaw, 0.0f, 360.0f);
  m_Pitch = repeat(m_Pitch + pitch, 0.0f, 360.0f);

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
