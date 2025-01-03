#pragma once

#include <QObject>
#include <QVector3D>

class Camera final : public QObject
{
  Q_OBJECT

public:
  explicit Camera(QObject* parent = nullptr) : QObject(parent)
  {
  }

  [[nodiscard]] QVector3D lookAt() const { return m_LookAt; }
  [[nodiscard]] float pitch() const { return m_Pitch; }
  [[nodiscard]] float yaw() const { return m_Yaw; }
  [[nodiscard]] float distance() const { return m_Distance; }
  [[nodiscard]] float nearPlane() const { return m_NearPlane; }
  [[nodiscard]] float farPlane() const { return m_FarPlane; }

  void setDistance(float distance);
  void setLookAt(QVector3D lookAt);

  void pan(QVector3D delta);
  void rotate(float yaw, float pitch);
  void zoomDistance(float distance);
  void zoomFactor(float factor);

private:
  static float repeat(float value, float min, float max);

  QVector3D m_LookAt;
  float m_Pitch     = 0.0f;
  float m_Yaw       = 0.0f;
  float m_Distance  = 500.0f;
  float m_NearPlane = 10.0f;
  float m_FarPlane  = 1000.0f;

signals:
  void cameraMoved();
};
