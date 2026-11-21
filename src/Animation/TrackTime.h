#pragma once

#include <cstdint>

namespace Animation {

enum class Cycle : std::uint8_t {
    kLoop,
    kReverse,
    kClamp,
};

struct TimeRange {
    float start = 0.0F;
    float stop = 0.0F;
    Cycle cycle = Cycle::kLoop;

    [[nodiscard]] float Length() const noexcept {
        return stop - start;
    }
};

// Where a time falls in a range that repeats, plays back and forth or holds its ends, as its cycle says.
[[nodiscard]] float CycledTime(const TimeRange& a_range, float a_time);

// How a controller turns scene time into the time its keys are sampled at.
struct ControllerTiming {
    float frequency = 1.0F;
    float phase = 0.0F;
    TimeRange range;
};

[[nodiscard]] float ControllerTime(const ControllerTiming& a_timing, float a_sceneTime);

// The scene time at which a timing that plays once reaches its end. Zero when it repeats or has no length.
[[nodiscard]] float PlayOnceEnd(const ControllerTiming& a_timing);

} // namespace Animation
