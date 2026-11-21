#include "Nif/SceneGraph.h"
#include "Nif/ShaderFlags.h"

#include <nifly/NifFile.hpp>

#include <algorithm>

SceneGraph::SceneGraph(const nifly::NifFile& a_nifFile)
    : _blockCount(a_nifFile.GetHeader().GetNumBlocks()) {
    if (auto* const root = a_nifFile.GetRootNode()) {
        Add(a_nifFile, a_nifFile.GetBlockID(root), -1);
    }
}

void SceneGraph::Add(const nifly::NifFile& a_nifFile, const std::uint32_t a_block, const int a_parent) {
    const auto& header = a_nifFile.GetHeader();
    auto* const object = header.GetBlock<nifly::NiAVObject>(a_block);
    if (object == nullptr) {
        return;
    }

    const auto index = static_cast<int>(_objects.size());
    _objects.push_back({
        .block = a_block,
        .parent = a_parent,
        .local = object->GetTransformToParent(),
        .hidden = (object->flags & TriShape::kHidden) != 0U,
    });

    if (auto* const node = dynamic_cast<nifly::NiNode*>(object)) {
        for (const auto& child : node->childRefs) {
            Add(a_nifFile, child.index, index);
        }
    }
}

std::vector<ObjectState> SceneGraph::Evaluate(const NodePose& a_pose) const {
    std::vector<ObjectState> states(_blockCount);
    for (const auto& object : _objects) {
        const auto* const animated = a_pose.Local(object.block);
        const auto& local = animated != nullptr ? *animated : object.local;
        const auto hidden = a_pose.Hidden(object.block).value_or(object.hidden);

        auto& state = states[object.block];
        const auto posed = animated != nullptr;
        if (object.parent < 0) {
            state = {.world = local, .hidden = hidden, .posed = posed};
        } else {
            const auto& parent = states[_objects[static_cast<std::size_t>(object.parent)].block];
            state = {
                .world = parent.world.ComposeTransforms(local),
                .hidden = hidden || parent.hidden,
                .posed = posed || parent.posed,
            };
        }
    }
    return states;
}

nifly::MatTransform SceneGraph::RestLocal(const std::uint32_t a_block) const {
    const auto object = std::ranges::find(_objects, a_block, &Object::block);
    return object != _objects.end() ? object->local : nifly::MatTransform {};
}
