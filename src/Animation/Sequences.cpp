#include "Animation/Sequences.h"
#include "Animation/ShaderChannels.h"

#include <nifly/NifFile.hpp>

namespace {
// A sequence names the node or shape a block moves or shows. A shader controller has its shader as its own target.
std::uint32_t TargetOf(
    const nifly::NifFile& a_nifFile,
    nifly::NiTimeController* const a_controller,
    const nifly::ControllerLink& a_link
) {
    if (Animation::ShaderFloatOf(a_controller) || Animation::ShaderColorOf(a_controller)) {
        return a_controller->targetRef.index;
    }
    auto* const object = a_nifFile.FindBlockByName<nifly::NiAVObject>(a_link.nodeName.get());
    return object != nullptr ? a_nifFile.GetBlockID(object) : nifly::NIF_NPOS;
}
} // namespace

namespace Animation {

std::vector<Sequence> SequencesOf(const TrackSource& a_source) {
    const auto& header = a_source.file.GetHeader();

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
                const auto target = TargetOf(a_source.file, controller, link);
                AddTrack(sequence.tracks, a_source, controller, target, header.GetBlock(link.interpolatorRef), timing);
            }
        }
        if (!sequence.tracks.Empty()) {
            sequences.push_back(std::move(sequence));
        }
    }
    return sequences;
}

} // namespace Animation
