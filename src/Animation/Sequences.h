#pragma once

#include "Animation/Tracks.h"

#include <string>
#include <vector>

namespace Animation {

// One of the named animations a controller manager holds, such as Open or Close.
struct Sequence {
    std::string name;
    bool loops = false;
    TrackSet tracks;
};

// The sequences that animate something the preview draws, in the file's order.
[[nodiscard]] std::vector<Sequence> SequencesOf(const TrackSource& a_source);

} // namespace Animation
