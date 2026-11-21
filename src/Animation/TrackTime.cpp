#include "Animation/TrackTime.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
// What files store for a controller without a range.
constexpr auto kUnsetStart = std::numeric_limits<float>::max();
constexpr auto kUnsetStop = std::numeric_limits<float>::lowest();

float Looped(const Animation::TimeRange& a_range, const float a_time) {
    const auto length = a_range.Length();
    if (length == 0.0F) {
        return a_range.start;
    }
    const auto time = a_range.start + std::fmod(a_time - a_range.start, length);
    return time < a_range.start ? time + length : time;
}

// The games count the back and forth from zero, not from the range's start.
float Reversed(const Animation::TimeRange& a_range, const float a_time) {
    const auto length = a_range.Length();
    if (length == 0.0F) {
        return a_range.start;
    }
    const auto period = 2.0F * length;
    auto into = std::fmod(a_time, period);
    if (into < 0.0F) {
        into += period;
    }
    return a_range.start + (into <= length ? into : period - into);
}
} // namespace

namespace Animation {

// Follows NiTimeController::ComputeScaledTime, which Skyrim and Fallout 4 share.
float CycledTime(const TimeRange& a_range, const float a_time) {
    auto time = a_time;
    if (a_range.start != kUnsetStart && a_range.stop != kUnsetStop) {
        switch (a_range.cycle) {
            case Cycle::kLoop:    time = Looped(a_range, a_time); break;
            case Cycle::kReverse: time = Reversed(a_range, a_time); break;
            case Cycle::kClamp:   break;
        }
    }

    // A time outside the range holds at the end it passed. The games test the stop first, which decides a range
    // whose stop is before its start.
    if (time > a_range.stop) {
        return a_range.stop;
    }
    return std::max(time, a_range.start);
}

float ControllerTime(const ControllerTiming& a_timing, const float a_sceneTime) {
    return CycledTime(a_timing.range, (a_timing.frequency * a_sceneTime) + a_timing.phase);
}

float PlayOnceEnd(const ControllerTiming& a_timing) {
    if (a_timing.range.cycle != Cycle::kClamp || a_timing.range.Length() <= 0.0F || a_timing.frequency <= 0.0F) {
        return 0.0F;
    }
    return (a_timing.range.stop - a_timing.phase) / a_timing.frequency;
}

} // namespace Animation
