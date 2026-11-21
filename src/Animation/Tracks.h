#pragma once

#include "Animation/Curves.h"
#include "Animation/ShaderValues.h"
#include "Animation/TrackTime.h"

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace nifly {
class NifFile;
class NiHeader;
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
    ColorCurve curve;
    ControllerTiming timing;
};

struct TrackSet {
    std::vector<ShaderFloatTrack> floats;
    std::vector<ShaderColorTrack> colors;

    [[nodiscard]] bool Empty() const noexcept {
        return floats.empty() && colors.empty();
    }
};

// Where the tracks of a file are looked up, and the shaders worth a track.
struct TrackSource {
    const nifly::NiHeader& header;
    const std::unordered_set<std::uint32_t>& drawnShaders;
};

// Adds the track a controller gives when it follows an interpolator with a timing. Adds nothing for a controller
// the preview doesn't animate, a shader it doesn't draw, or an interpolator without a curve.
void AddTrack(
    TrackSet& a_tracks,
    const TrackSource& a_source,
    nifly::NiTimeController* a_controller,
    nifly::NiInterpolator* a_interpolator,
    const ControllerTiming& a_timing
);

// The tracks of the controllers that run by themselves. A controller that a controller manager's sequences drive is
// left to its sequences.
[[nodiscard]] TrackSet SelfRunningTracks(const TrackSource& a_source);

// How long the tracks that play once take together, in scene seconds. Zero when none does.
[[nodiscard]] float PlayOnceLength(const TrackSet& a_tracks);

} // namespace Animation
