#include "Camera.h"

#include <cmath>

void Camera::setDistance(const float distance)
{
  m_Distance = qBound(MinDistance, distance, MaxDistance);
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
  m_Distance += distance;
  m_Distance = qBound(MinDistance, m_Distance, MaxDistance);

  cameraMoved();
}

void Camera::zoomFactor(const float factor)
{
  m_Distance *= factor;
  m_Distance = qBound(MinDistance, m_Distance, MaxDistance);

  cameraMoved();
}

float Camera::repeat(const float value, const float min, const float max)
{
  return fmod(fmod(value, max - min) + (max - min), max - min) + min;
}
