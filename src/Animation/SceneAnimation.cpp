#include "Animation/SceneAnimation.h"
#include "Animation/DrawnTargets.h"

#include <nifly/NifFile.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>

namespace {
// How long tracks that play once keep their last pose before they start over. Some last a fraction of a second, such
// as a lid opening, and would flicker without it.
constexpr float kHoldSeconds = 1.0F;

bool NamesLoop(const Animation::Sequence& a_sequence) {
    constexpr std::string_view kLoop = "loop";
    return !std::ranges::search(a_sequence.name, kLoop, {}, [](const unsigned char a_letter) {
        return static_cast<char>(std::tolower(a_letter));
    }).empty();
}

// The sequence a preview opens on: one that keeps showing something. Files name those with "Loop", and mark some of
// them as playing once, so the name counts before the cycle.
std::size_t OpeningSequence(const std::vector<Animation::Sequence>& a_sequences) {
    auto opening = std::ranges::find_if(a_sequences, NamesLoop);
    if (opening == a_sequences.end()) {
        opening = std::ranges::find_if(a_sequences, &Animation::Sequence::loops);
    }
    return opening != a_sequences.end() ? static_cast<std::size_t>(std::distance(a_sequences.begin(), opening)) : 0;
}
} // namespace

namespace Animation {

SceneAnimation::SceneAnimation(nifly::NifFile& a_nifFile) {
    const DrawnTargets drawn(a_nifFile);
    const TrackSource source {.file = a_nifFile, .drawn = drawn};
    _selfRunning = SelfRunningTracks(source);
    _sequences = SequencesOf(source);
    _playing = OpeningSequence(_sequences);
    UpdateReplayPeriod();
}

const Sequence* SceneAnimation::PlayingSequence() const noexcept {
    return _playing < _sequences.size() ? &_sequences[_playing] : nullptr;
}

void SceneAnimation::PlaySequence(const std::string& a_name) {
    const auto named = std::ranges::find(_sequences, a_name, &Sequence::name);
    if (named == _sequences.end()) {
        return;
    }

    _playing = static_cast<std::size_t>(std::distance(_sequences.begin(), named));
    UpdateReplayPeriod();
}

Pose SceneAnimation::At(const float a_sceneTime) const {
    Pose pose;
    AddValues(pose, _selfRunning, a_sceneTime);
    if (const auto* const sequence = PlayingSequence()) {
        AddValues(pose, sequence->tracks, a_sceneTime);
    }
    return pose;
}

void SceneAnimation::AddValues(Pose& a_pose, const TrackSet& a_tracks, const float a_sceneTime) const {
    for (const auto& track : a_tracks.floats) {
        a_pose.ShaderToAnimate(track.shader)[track.value] = track.curve.At(TrackTime(track.timing, a_sceneTime));
    }
    for (const auto& track : a_tracks.colors) {
        a_pose.ShaderToAnimate(track.shader)[track.value] = track.curve.At(TrackTime(track.timing, a_sceneTime));
    }
    for (const auto& track : a_tracks.transforms) {
        // A second track on the same object changes its own parts and keeps what the first one set.
        const auto* const posed = a_pose.Nodes().Local(track.object);
        auto transform = posed != nullptr ? *posed : track.stored;
        track.curve.Apply(transform, TrackTime(track.timing, a_sceneTime));
        a_pose.Nodes().SetLocal(track.object, transform);
    }
    for (const auto& track : a_tracks.visibilities) {
        a_pose.Nodes().SetHidden(track.object, !track.curve.At(TrackTime(track.timing, a_sceneTime)));
    }
}

float SceneAnimation::TrackTime(const ControllerTiming& a_timing, const float a_sceneTime) const {
    const auto replays = a_timing.range.cycle == Cycle::kClamp && _replayPeriod > 0.0F;
    return ControllerTime(a_timing, replays ? std::fmod(a_sceneTime, _replayPeriod) : a_sceneTime);
}

void SceneAnimation::UpdateReplayPeriod() {
    auto length = PlayOnceLength(_selfRunning);
    if (const auto* const sequence = PlayingSequence()) {
        length = std::max(length, PlayOnceLength(sequence->tracks));
    }
    _replayPeriod = length > 0.0F ? length + kHoldSeconds : 0.0F;
}

} // namespace Animation
