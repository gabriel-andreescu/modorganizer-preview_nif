#include "Preview/AnimationClock.h"

namespace {
// About sixty frames a second.
constexpr int kFrameMilliseconds = 16;
constexpr float kMillisecondsPerSecond = 1000.0F;
} // namespace

AnimationClock::AnimationClock(QObject* a_parent)
    : QObject(a_parent) {
    _frameTimer.setInterval(kFrameMilliseconds);
    connect(&_frameTimer, &QTimer::timeout, this, [this] { emit Ticked(Seconds()); });
}

void AnimationClock::SetRunning(const bool a_running) {
    if (a_running == _frameTimer.isActive()) {
        return;
    }

    if (a_running) {
        _sinceStarted.start();
        _frameTimer.start();
    } else {
        _playedBefore += _sinceStarted.elapsed();
        _frameTimer.stop();
    }
}

void AnimationClock::Restart() {
    _playedBefore = 0;
    if (_frameTimer.isActive()) {
        _sinceStarted.start();
    }
}

float AnimationClock::Seconds() const {
    const auto played = _playedBefore + (_frameTimer.isActive() ? _sinceStarted.elapsed() : 0);
    return static_cast<float>(played) / kMillisecondsPerSecond;
}
