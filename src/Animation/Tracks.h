#pragma once

#include "Animation/Curves.h"
#include "Animation/DrawnTargets.h"
#include "Animation/ShaderValues.h"
#include "Animation/TrackTime.h"

#include <cstdint>
#include <vector>

namespace nifly {
class NifFile;
}

namespace Animation {

// One animated shader value: the shader block it belongs to and the curve it follows.
struct ShaderFloatTrack {
    std::uint32_t shader = 0;
    ShaderFloat value = ShaderFloat::kAlpha;
    FloatCurve curve;
    ControllerTiming timing;
};

struct ShaderColorTrack {
    std::uint32_t shader = 0;
    ShaderColor value = ShaderColor::kEmissive;
    PointCurve curve;
    ControllerTiming timing;
};

// A node or shape that moves. The curve replaces parts of the transform the object stores.
struct TransformTrack {
    std::uint32_t object = 0;
    nifly::MatTransform stored;
    TransformCurve curve;
    ControllerTiming timing;
};

// A node or shape that appears and disappears.
struct VisibilityTrack {
    std::uint32_t object = 0;
    BoolCurve curve;
    ControllerTiming timing;
};

struct TrackSet {
    std::vector<ShaderFloatTrack> floats;
    std::vector<ShaderColorTrack> colors;
    std::vector<TransformTrack> transforms;
    std::vector<VisibilityTrack> visibilities;

    [[nodiscard]] bool Empty() const noexcept {
        return floats.empty() && colors.empty() && transforms.empty() && visibilities.empty();
    }
};

// The file the tracks are read from, and what is worth a track in it.
struct TrackSource {
    const nifly::NifFile& file;
    const DrawnTargets& drawn;
};

// Adds the track a controller gives when it drives a target block from an interpolator with a timing. Adds nothing
// for a controller the preview doesn't animate, a target it doesn't draw, or an interpolator without a curve.
void AddTrack(
    TrackSet& a_tracks,
    const TrackSource& a_source,
    nifly::NiTimeController* a_controller,
    std::uint32_t a_target,
    nifly::NiInterpolator* a_interpolator,
    const ControllerTiming& a_timing
);

// The tracks of the controllers that run by themselves. A controller that a controller manager's sequences drive is
// left to its sequences.
[[nodiscard]] TrackSet SelfRunningTracks(const TrackSource& a_source);

// How long the tracks that play once take together, in scene seconds. Zero when none does.
[[nodiscard]] float PlayOnceLength(const TrackSet& a_tracks);

} // namespace Animation
