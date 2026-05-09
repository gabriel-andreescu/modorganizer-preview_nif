#pragma once

#include <QObject>
#include <QVector3D>

struct CameraState
{
  QVector3D lookAt;
  float pitch    = 0.0f;
  float yaw      = 0.0f;
  float distance = 500.0f;
};

class Camera final : public QObject
{
  Q_OBJECT

public:
  explicit Camera(QObject* parent = nullptr) : QObject(parent) {}

  [[nodiscard]] QVector3D lookAt() const { return m_LookAt; }
  [[nodiscard]] float pitch() const { return m_Pitch; }
  [[nodiscard]] float yaw() const { return m_Yaw; }
  [[nodiscard]] float distance() const { return m_Distance; }
  [[nodiscard]] float nearPlane() const { return m_NearPlane; }
  [[nodiscard]] float farPlane() const { return m_FarPlane; }
  [[nodiscard]] bool hasState() const { return m_HasState; }
  [[nodiscard]] CameraState state() const;

  void setState(const CameraState& state);
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
  bool m_HasState   = false;

signals:
  void cameraMoved();
};
