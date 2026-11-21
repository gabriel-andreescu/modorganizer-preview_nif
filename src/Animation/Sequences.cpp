#include "Animation/Sequences.h"

#include <nifly/NifFile.hpp>

namespace Animation {

std::vector<Sequence> SequencesOf(const TrackSource& a_source) {
    const auto& header = a_source.header;

    std::vector<Sequence> sequences;
    for (std::uint32_t id = 0; id < header.GetNumBlocks(); ++id) {
        auto* const block = header.GetBlock<nifly::NiControllerSequence>(id);
        if (block == nullptr) {
            continue;
        }

        // A sequence's blocks all play on the sequence's own time, whatever their controllers store. The games loop
        // a sequence or hold its ends, and play a back-and-forth one once like the rest.
        const auto loops = block->cycleType == nifly::CYCLE_LOOP;
        const ControllerTiming timing {
            .frequency = block->frequency,
            .range = {
                .start = block->startTime,
                .stop = block->stopTime,
                .cycle = loops ? Cycle::kLoop : Cycle::kClamp,
            },
        };
        Sequence sequence {.name = block->name.get(), .loops = loops};
        for (const auto& link : block->controlledBlocks) {
            if (auto* const controller = header.GetBlock(link.controllerRef)) {
                AddTrack(sequence.tracks, a_source, controller, header.GetBlock(link.interpolatorRef), timing);
            }
        }
        if (!sequence.tracks.Empty()) {
            sequences.push_back(std::move(sequence));
        }
    }
    return sequences;
}

} // namespace Animation
