#pragma once

#include "Animation/Pose.h"
#include "Animation/Sequences.h"
#include "Animation/Tracks.h"

#include <string>
#include <vector>

namespace nifly {
class NifFile;
}

namespace Animation {

// A NIF's animation: the controllers that run by themselves, and one of its named sequences at a time. The file
// must outlive it, since the tracks read the file's keys.
class SceneAnimation {
public:
    // No animation, for a NIF whose controllers are not played.
    SceneAnimation() = default;
    explicit SceneAnimation(nifly::NifFile& a_nifFile);

    [[nodiscard]] bool IsAnimated() const noexcept {
        return !_selfRunning.Empty() || !_sequences.empty();
    }
    [[nodiscard]] const std::vector<Sequence>& Sequences() const noexcept {
        return _sequences;
    }
    // The sequence that plays, or null when the file has none.
    [[nodiscard]] const Sequence* PlayingSequence() const noexcept;
    // Plays the sequence with this name. A name the file doesn't have changes nothing.
    void PlaySequence(const std::string& a_name);
    [[nodiscard]] Pose At(float a_sceneTime) const;

private:
    void AddValues(Pose& a_pose, const TrackSet& a_tracks, float a_sceneTime) const;
    [[nodiscard]] float TrackTime(const ControllerTiming& a_timing, float a_sceneTime) const;
    void UpdateReplayPeriod();

    TrackSet _selfRunning;
    std::vector<Sequence> _sequences;
    std::size_t _playing = 0;
    // How long the tracks that play once take together, and hold their last pose. They start over after it, so the
    // preview keeps showing them. Zero when there are none.
    float _replayPeriod = 0.0F;
};

} // namespace Animation
