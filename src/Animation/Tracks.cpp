#include "Animation/Tracks.h"
#include "Animation/ShaderChannels.h"

#include <nifly/NifFile.hpp>

#include <algorithm>
#include <ranges>

namespace {
constexpr unsigned kCycleMask = 0x0006U;
constexpr unsigned kActiveFlag = 0x0008U;
// A controller manager owns the controller's time, so the games never run it by itself.
constexpr unsigned kManagerControlledFlag = 0x0020U;

Animation::Cycle CycleOf(const unsigned a_flags) {
    switch ((a_flags & kCycleMask) >> 1U) {
        case 1:  return Animation::Cycle::kReverse;
        case 2:  return Animation::Cycle::kClamp;
        default: return Animation::Cycle::kLoop;
    }
}

Animation::ControllerTiming TimingOf(const nifly::NiTimeController& a_controller) {
    return {
        .frequency = a_controller.frequency,
        .phase = a_controller.phase,
        .range = {
            .start = a_controller.startTime,
            .stop = a_controller.stopTime,
            .cycle = CycleOf(a_controller.flags),
        },
    };
}

bool RunsByItself(const nifly::NiTimeController& a_controller) {
    return (a_controller.flags & kActiveFlag) != 0 && (a_controller.flags & kManagerControlledFlag) == 0;
}

std::unordered_set<std::uint32_t> SequencedControllers(const nifly::NiHeader& a_header) {
    std::unordered_set<std::uint32_t> controllers;
    for (std::uint32_t id = 0; id < a_header.GetNumBlocks(); ++id) {
        if (const auto* const sequence = a_header.GetBlock<nifly::NiControllerSequence>(id)) {
            // The library's list has no begin() for a const list, so the loop takes its const iterators.
            const auto& links = sequence->controlledBlocks;
            for (const auto& link : std::ranges::subrange(links.cbegin(), links.cend())) {
                controllers.insert(link.controllerRef.index);
            }
        }
    }
    return controllers;
}
} // namespace

namespace Animation {

void AddTrack(
    TrackSet& a_tracks,
    const TrackSource& a_source,
    nifly::NiTimeController* const a_controller,
    nifly::NiInterpolator* const a_interpolator,
    const ControllerTiming& a_timing
) {
    const auto shader = a_controller->targetRef.index;
    if (!a_source.drawnShaders.contains(shader)) {
        return;
    }

    if (const auto value = ShaderFloatOf(a_controller)) {
        if (const auto curve = FloatCurveOf(a_source.header, a_interpolator)) {
            a_tracks.floats.push_back({.shader = shader, .value = *value, .curve = *curve, .timing = a_timing});
        }
    } else if (const auto color = ShaderColorOf(a_controller)) {
        if (const auto curve = ColorCurveOf(a_source.header, a_interpolator)) {
            a_tracks.colors.push_back({.shader = shader, .value = *color, .curve = *curve, .timing = a_timing});
        }
    }
}

TrackSet SelfRunningTracks(const TrackSource& a_source) {
    const auto& header = a_source.header;
    const auto sequenced = SequencedControllers(header);

    TrackSet tracks;
    for (std::uint32_t id = 0; id < header.GetNumBlocks(); ++id) {
        auto* const controller = header.GetBlock<nifly::NiSingleInterpController>(id);
        if (controller == nullptr || !RunsByItself(*controller) || sequenced.contains(id)) {
            continue;
        }

        // The library types this reference as a controller, so it is resolved by its index.
        auto* const interpolator = header.GetBlock<nifly::NiInterpolator>(controller->interpolatorRef.index);
        AddTrack(tracks, a_source, controller, interpolator, TimingOf(*controller));
    }
    return tracks;
}

float PlayOnceLength(const TrackSet& a_tracks) {
    auto length = 0.0F;
    for (const auto& track : a_tracks.floats) {
        length = std::max(length, PlayOnceEnd(track.timing));
    }
    for (const auto& track : a_tracks.colors) {
        length = std::max(length, PlayOnceEnd(track.timing));
    }
    return length;
}

} // namespace Animation
