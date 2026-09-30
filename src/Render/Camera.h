#pragma once

#include <QObject>
#include <QVector3D>

struct CameraState {
    QVector3D lookAt;
    float pitch = 0.0F;
    float yaw = 0.0F;
    float distance = 500.0F;
};

class Camera final : public QObject {
    Q_OBJECT

public:
    explicit Camera(QObject* a_parent = nullptr)
        : QObject(a_parent) {}

    [[nodiscard]] QVector3D LookAt() const {
        return _lookAt;
    }
    [[nodiscard]] float Pitch() const {
        return _pitch;
    }
    [[nodiscard]] float Yaw() const {
        return _yaw;
    }
    [[nodiscard]] float Distance() const {
        return _distance;
    }
    [[nodiscard]] float NearPlane() const {
        return _nearPlane;
    }
    [[nodiscard]] float FarPlane() const {
        return _farPlane;
    }
    [[nodiscard]] bool HasState() const {
        return _hasState;
    }
    [[nodiscard]] CameraState State() const;

    void SetState(const CameraState& a_state);
    void SetDistance(float a_distance);
    void SetLookAt(QVector3D a_lookAt);

    void Pan(QVector3D a_delta);
    void Rotate(float a_yaw, float a_pitch);
    void ZoomDistance(float a_distance);
    void ZoomFactor(float a_factor);

private:
    static float Repeat(float a_value, float a_min, float a_max);

    QVector3D _lookAt;
    float _pitch = 0.0F;
    float _yaw = 0.0F;
    float _distance = 500.0F;
    float _nearPlane = 10.0F;
    float _farPlane = 1000.0F;
    bool _hasState = false;

signals:
    void CameraMoved();
};
