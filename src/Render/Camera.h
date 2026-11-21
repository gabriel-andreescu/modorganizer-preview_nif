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
    [[nodiscard]] bool HasState() const {
        return _hasState;
    }
    // Framed cameras keep fitting the mesh as the viewport changes, until panned or zoomed.
    [[nodiscard]] bool IsFramed() const {
        return _framed;
    }
    [[nodiscard]] CameraState State() const;

    void SetState(const CameraState& a_state);
    void Frame(const CameraState& a_state);
    void Refit(float a_distance);
    void SetDistance(float a_distance);
    void SetLookAt(QVector3D a_lookAt);

    void Pan(QVector3D a_delta);
    void Rotate(float a_yaw, float a_pitch);
    // Turns the view to these angles and keeps its distance and the point it looks at.
    void SetAngles(float a_yaw, float a_pitch);
    void ZoomDistance(float a_distance);
    void ZoomFactor(float a_factor);

private:
    static float Repeat(float a_value, float a_min, float a_max);

    QVector3D _lookAt;
    float _pitch = 0.0F;
    float _yaw = 0.0F;
    float _distance = 500.0F;
    bool _hasState = false;
    bool _framed = false;

signals:
    void CameraMoved();
};
