#pragma once

#include "Nif/NodePose.h"

#include <nifly/Object3d.hpp>

#include <cstdint>
#include <vector>

namespace nifly {
class NifFile;
}

// Where one object sits in the scene and whether it shows, itself or through a hidden parent.
struct ObjectState {
    nifly::MatTransform world;
    bool hidden = false;
    // Whether the pose moves the object, itself or through a parent.
    bool posed = false;
};

// The nodes and shapes of a NIF as a tree, to place them for a pose.
class SceneGraph {
public:
    explicit SceneGraph(const nifly::NifFile& a_nifFile);

    // The state of every object for a pose, by block. Blocks that are not in the tree keep the default state.
    [[nodiscard]] std::vector<ObjectState> Evaluate(const NodePose& a_pose) const;
    // The transform the NIF stores for an object, or the identity for a block that is not in the tree.
    [[nodiscard]] nifly::MatTransform RestLocal(std::uint32_t a_block) const;

private:
    struct Object {
        std::uint32_t block = 0;
        // The parent's place in the list, which is before its children. Negative for a root.
        int parent = -1;
        nifly::MatTransform local;
        bool hidden = false;
    };

    void Add(const nifly::NifFile& a_nifFile, std::uint32_t a_block, int a_parent);

    std::vector<Object> _objects;
    std::uint32_t _blockCount = 0;
};
