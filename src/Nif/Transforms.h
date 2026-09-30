#pragma once

#include <nifly/NifFile.hpp>

#include <cstdint>

inline nifly::MatTransform GetObjectTransformToGlobal(const nifly::NifFile* a_nifFile, nifly::NiAVObject* a_object) {
    nifly::MatTransform xform = a_object->GetTransformToParent();
    nifly::NiNode* parent = a_nifFile->GetParentNode(a_object);
    while (parent) {
        xform = parent->GetTransformToParent().ComposeTransforms(xform);
        parent = a_nifFile->GetParentNode(parent);
    }

    return xform;
}

inline nifly::MatTransform GetShapeTransformToGlobal(const nifly::NifFile* a_nifFile, nifly::NiShape* a_niShape) {
    return GetObjectTransformToGlobal(a_nifFile, a_niShape);
}

inline bool GetNodeTransformToAncestor(
    const nifly::NifFile* a_nifFile,
    nifly::NiNode* a_node,
    const std::uint32_t a_ancestorId,
    nifly::MatTransform& a_transform
) {
    a_transform.Clear();

    while (a_node) {
        const auto nodeId = a_nifFile->GetBlockID(a_node);
        if (nodeId == a_ancestorId) {
            return true;
        }

        a_transform = a_node->GetTransformToParent().ComposeTransforms(a_transform);
        a_node = a_nifFile->GetParentNode(a_node);
    }

    return false;
}

inline nifly::BoundingSphere GetBoundingSphere(nifly::NifFile* a_nifFile, nifly::NiShape* a_niShape) {
    if (const auto* const vertices = a_nifFile->GetVertsForShape(a_niShape)) {
        auto bounds = nifly::BoundingSphere(*vertices);

        const auto xform = GetShapeTransformToGlobal(a_nifFile, a_niShape);

        bounds.center = xform.ApplyTransform(bounds.center);
        bounds.radius = xform.ApplyTransformToDist(bounds.radius);
        return bounds;
    }

    return {};
}
