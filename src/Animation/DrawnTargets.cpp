#include "Animation/DrawnTargets.h"
#include "Nif/ShapeVisibility.h"

#include <nifly/NifFile.hpp>

#include <algorithm>

namespace Animation {

DrawnTargets::DrawnTargets(const nifly::NifFile& a_nifFile) {
    const auto& header = a_nifFile.GetHeader();
    const auto hiddenCanAppear = AnimatesVisibility(a_nifFile);
    for (auto* const shape : a_nifFile.GetShapes()) {
        if (shape == nullptr) {
            continue;
        }
        const auto visibility = ClassifyShapeVisibility(a_nifFile, shape);
        if (visibility == ShapeVisibility::kInactiveSwitchChild
            || (visibility == ShapeVisibility::kHidden && !hiddenCanAppear)) {
            continue;
        }

        if (auto* const shader = a_nifFile.GetShader(shape)) {
            _shaders.push_back(a_nifFile.GetBlockID(shader));
        }
        AddWithParents(a_nifFile, shape);
        // A skinned shape's vertices follow its bones, which hang elsewhere in the tree.
        if (const auto* const skin = header.GetBlock<nifly::NiBoneContainer>(shape->SkinInstanceRef())) {
            for (std::uint32_t bone = 0; bone < skin->boneRefs.GetSize(); ++bone) {
                AddWithParents(a_nifFile, header.GetBlock<nifly::NiNode>(skin->boneRefs.GetBlockRef(bone)));
            }
        }
    }
    std::ranges::sort(_shaders);
    std::ranges::sort(_objects);
}

void DrawnTargets::AddWithParents(const nifly::NifFile& a_nifFile, nifly::NiAVObject* a_object) {
    for (; a_object != nullptr; a_object = a_nifFile.GetParentNode(a_object)) {
        _objects.push_back(a_nifFile.GetBlockID(a_object));
    }
}

bool DrawnTargets::HasShader(const std::uint32_t a_block) const {
    return std::ranges::binary_search(_shaders, a_block);
}

bool DrawnTargets::HasObject(const std::uint32_t a_block) const {
    return std::ranges::binary_search(_objects, a_block);
}

bool AnimatesVisibility(const nifly::NifFile& a_nifFile) {
    const auto& header = a_nifFile.GetHeader();
    for (std::uint32_t id = 0; id < header.GetNumBlocks(); ++id) {
        if (header.GetBlock<nifly::NiVisController>(id) != nullptr) {
            return true;
        }
    }
    return false;
}

} // namespace Animation
