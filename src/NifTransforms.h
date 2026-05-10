#pragma once

#include <NifFile.hpp>

#include <cstdint>

inline nifly::MatTransform GetObjectTransformToGlobal(const nifly::NifFile* nifFile, nifly::NiAVObject* object) {
    nifly::MatTransform xform = object->GetTransformToParent();
    nifly::NiNode* parent = nifFile->GetParentNode(object);
    while (parent) {
        xform = parent->GetTransformToParent().ComposeTransforms(xform);
        parent = nifFile->GetParentNode(parent);
    }

    return xform;
}

inline nifly::MatTransform GetShapeTransformToGlobal(const nifly::NifFile* nifFile, nifly::NiShape* niShape) {
    return GetObjectTransformToGlobal(nifFile, niShape);
}

inline bool GetNodeTransformToAncestor(
    const nifly::NifFile* nifFile,
    nifly::NiNode* node,
    const std::uint32_t ancestorId,
    nifly::MatTransform& transform
) {
    transform.Clear();

    while (node) {
        const auto nodeId = nifFile->GetBlockID(node);
        if (nodeId == ancestorId) {
            return true;
        }

        transform = node->GetTransformToParent().ComposeTransforms(transform);
        node = nifFile->GetParentNode(node);
    }

    return false;
}

inline nifly::BoundingSphere GetBoundingSphere(nifly::NifFile* nifFile, nifly::NiShape* niShape) {
    if (const auto vertices = nifFile->GetVertsForShape(niShape)) {
        auto bounds = nifly::BoundingSphere(*vertices);

        const auto xform = GetShapeTransformToGlobal(nifFile, niShape);

        bounds.center = xform.ApplyTransform(bounds.center);
        bounds.radius = xform.ApplyTransformToDist(bounds.radius);
        return bounds;
    }

    return {};
}
