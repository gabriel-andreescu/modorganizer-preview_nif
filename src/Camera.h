#pragma once

#include <QObject>
#include <QVector3D>

class Camera final : public QObject
{
  Q_OBJECT

public:
  explicit Camera(QObject* parent = nullptr) : QObject(parent) {}
  [[nodiscard]] QVector3D lookAt() const { return m_LookAt; }
  [[nodiscard]] float pitch() const { return m_Pitch; }
  [[nodiscard]] float yaw() const { return m_Yaw; }
  [[nodiscard]] float distance() const { return m_Distance; }

  void setDistance(float distance);
  void setLookAt(QVector3D lookAt);

  void pan(QVector3D delta);
  void rotate(float yaw, float pitch);
  void zoomDistance(float distance);
  void zoomFactor(float factor);

private:
  static constexpr float MinDistance = 1.0f;
  static constexpr float MaxDistance = 10000.0f;

  static float repeat(float value, float min, float max);

  QVector3D m_LookAt;
  float m_Pitch    = 0.0f;
  float m_Yaw      = 0.0f;
  float m_Distance = 100.0f;

signals:
  void cameraMoved();
};
