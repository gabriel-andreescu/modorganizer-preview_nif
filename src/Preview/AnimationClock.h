#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

// Counts the seconds an animation has played, and ticks once a frame while it runs.
class AnimationClock final : public QObject {
    Q_OBJECT

public:
    explicit AnimationClock(QObject* a_parent = nullptr);

    void SetRunning(bool a_running);
    // Counts from zero again.
    void Restart();
    [[nodiscard]] float Seconds() const;

signals:
    void Ticked(float a_seconds);

private:
    QTimer _frameTimer;
    QElapsedTimer _sinceStarted;
    qint64 _playedBefore = 0;
};
