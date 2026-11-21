#include "Animation/Tracks.h"
#include "Animation/ShaderChannels.h"

#include <nifly/NifFile.hpp>

#include <algorithm>
#include <ranges>
#include <unordered_set>

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

bool MovesObjects(nifly::NiTimeController* const a_controller) {
    return dynamic_cast<nifly::NiTransformController*>(a_controller) != nullptr
           || dynamic_cast<nifly::NiMultiTargetTransformController*>(a_controller) != nullptr;
}

void AddShaderTrack(
    Animation::TrackSet& a_tracks,
    const nifly::NiHeader& a_header,
    nifly::NiTimeController* const a_controller,
    const std::uint32_t a_shader,
    nifly::NiInterpolator* const a_interpolator,
    const Animation::ControllerTiming& a_timing
) {
    if (const auto value = Animation::ShaderFloatOf(a_controller)) {
        if (const auto curve = Animation::FloatCurveOf(a_header, a_interpolator)) {
            a_tracks.floats.push_back({.shader = a_shader, .value = *value, .curve = *curve, .timing = a_timing});
        }
    } else if (const auto color = Animation::ShaderColorOf(a_controller)) {
        if (const auto curve = Animation::PointCurveOf(a_header, a_interpolator)) {
            a_tracks.colors.push_back({.shader = a_shader, .value = *color, .curve = *curve, .timing = a_timing});
        }
    }
}

void AddObjectTrack(
    Animation::TrackSet& a_tracks,
    const nifly::NiHeader& a_header,
    nifly::NiTimeController* const a_controller,
    const std::uint32_t a_object,
    nifly::NiInterpolator* const a_interpolator,
    const Animation::ControllerTiming& a_timing
) {
    if (MovesObjects(a_controller)) {
        const auto* const object = a_header.GetBlock<nifly::NiAVObject>(a_object);
        if (const auto curve = Animation::TransformCurveOf(a_header, a_interpolator)) {
            a_tracks.transforms.push_back({
                .object = a_object,
                .stored = object->GetTransformToParent(),
                .curve = *curve,
                .timing = a_timing,
            });
        }
    } else if (dynamic_cast<nifly::NiVisController*>(a_controller) != nullptr) {
        if (const auto curve = Animation::BoolCurveOf(a_header, a_interpolator)) {
            a_tracks.visibilities.push_back({.object = a_object, .curve = *curve, .timing = a_timing});
        }
    }
}
} // namespace

namespace Animation {

void AddTrack(
    TrackSet& a_tracks,
    const TrackSource& a_source,
    nifly::NiTimeController* const a_controller,
    const std::uint32_t a_target,
    nifly::NiInterpolator* const a_interpolator,
    const ControllerTiming& a_timing
) {
    const auto& header = a_source.file.GetHeader();
    if (a_source.drawn.HasShader(a_target)) {
        AddShaderTrack(a_tracks, header, a_controller, a_target, a_interpolator, a_timing);
    } else if (a_source.drawn.HasObject(a_target)) {
        AddObjectTrack(a_tracks, header, a_controller, a_target, a_interpolator, a_timing);
    }
}

TrackSet SelfRunningTracks(const TrackSource& a_source) {
    const auto& header = a_source.file.GetHeader();
    const auto sequenced = SequencedControllers(header);

    TrackSet tracks;
    for (std::uint32_t id = 0; id < header.GetNumBlocks(); ++id) {
        auto* const controller = header.GetBlock<nifly::NiSingleInterpController>(id);
        if (controller == nullptr || !RunsByItself(*controller) || sequenced.contains(id)) {
            continue;
        }

        // The library types this reference as a controller, so it is resolved by its index.
        auto* const interpolator = header.GetBlock<nifly::NiInterpolator>(controller->interpolatorRef.index);
        AddTrack(tracks, a_source, controller, controller->targetRef.index, interpolator, TimingOf(*controller));
    }
    return tracks;
}

float PlayOnceLength(const TrackSet& a_tracks) {
    auto length = 0.0F;
    const auto extend = [&](const auto& a_list) {
        for (const auto& track : a_list) {
            length = std::max(length, PlayOnceEnd(track.timing));
        }
    };
    extend(a_tracks.floats);
    extend(a_tracks.colors);
    extend(a_tracks.transforms);
    extend(a_tracks.visibilities);
    return length;
}

} // namespace Animation
