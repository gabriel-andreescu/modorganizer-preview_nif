#include "Render/ShapeSkin.h"
#include "Nif/SceneGraph.h"
#include "Nif/Transforms.h"

#include <nifly/NifFile.hpp>
#include <nifly/Skin.hpp>

#include <algorithm>
#include <unordered_map>

namespace {
constexpr float kMinSkinWeight = 0.000001F;

bool IsLegacyTriShape(const nifly::NiShape* a_shape) {
    return a_shape->HasType<nifly::NiTriShape>() || a_shape->HasType<nifly::NiTriStrips>();
}

bool HasSameSize(const std::vector<nifly::Vector3>* a_values, const std::vector<nifly::Vector3>* a_positions) {
    return (a_values != nullptr) && (a_positions != nullptr) && a_values->size() == a_positions->size();
}

float PartitionWeight(const nifly::VertexWeight& a_weights, const std::size_t a_slot) {
    switch (a_slot) {
        case 0:  return a_weights.w1;
        case 1:  return a_weights.w2;
        case 2:  return a_weights.w3;
        case 3:  return a_weights.w4;
        default: return 0.0F;
    }
}

std::uint8_t PartitionBoneIndex(const nifly::BoneIndices& a_indices, const std::size_t a_slot) {
    switch (a_slot) {
        case 0:  return a_indices.i1;
        case 1:  return a_indices.i2;
        case 2:  return a_indices.i3;
        case 3:  return a_indices.i4;
        default: return 0;
    }
}

bool HasPartitionWeights(const nifly::NiSkinPartition* a_skinPartition) {
    if (!a_skinPartition) {
        return false;
    }

    return std::ranges::any_of(
        a_skinPartition->partitions,
        [](const nifly::NiSkinPartition::PartitionBlock& a_partition) {
            return a_partition.numWeightsPerVertex > 0
                   && !a_partition.bones.empty()
                   && !a_partition.vertexWeights.empty()
                   && !a_partition.boneIndices.empty();
        }
    );
}

std::vector<nifly::BoneIndices>::size_type PartitionVertexLimit(
    const nifly::NiSkinPartition::PartitionBlock& a_partition
) {
    auto limit = a_partition.boneIndices.size();
    limit = std::min(limit, a_partition.vertexWeights.size());
    if (a_partition.hasVertexMap) {
        limit = std::min(limit, a_partition.vertexMap.size());
    }
    if (a_partition.numVertices > 0) {
        limit = std::min(limit, static_cast<std::size_t>(a_partition.numVertices));
    }
    return limit;
}

void NormalizeVectors(std::vector<nifly::Vector3>& a_values) {
    for (auto& value : a_values) {
        if (!value.IsZero(true)) {
            value.Normalize();
        }
    }
}

void NormalizeDirections(SkinnedVertices& a_vertices) {
    NormalizeVectors(a_vertices.normals);
    NormalizeVectors(a_vertices.tangents);
    NormalizeVectors(a_vertices.bitangents);
}

SkinnedVertices ZeroedVertices(const SkinSource& a_source) {
    SkinnedVertices vertices;
    vertices.positions.assign(a_source.positions->size(), {});
    if (HasSameSize(a_source.normals, a_source.positions)) {
        vertices.normals.assign(a_source.positions->size(), {});
    }
    if (HasSameSize(a_source.tangents, a_source.positions)) {
        vertices.tangents.assign(a_source.positions->size(), {});
    }
    if (HasSameSize(a_source.bitangents, a_source.positions)) {
        vertices.bitangents.assign(a_source.positions->size(), {});
    }
    return vertices;
}

void AddDirection(
    const std::vector<nifly::Vector3>* a_source,
    std::vector<nifly::Vector3>& a_destination,
    const std::size_t a_vertex,
    const nifly::MatTransform& a_transform,
    const float a_weight
) {
    if (a_source && a_vertex < a_source->size() && a_vertex < a_destination.size()) {
        a_destination[a_vertex] += a_transform.ApplyTransformToDir((*a_source)[a_vertex]) * a_weight;
    }
}

void PlaceDirection(
    const std::vector<nifly::Vector3>* a_source,
    std::vector<nifly::Vector3>& a_destination,
    const std::size_t a_vertex,
    const nifly::MatTransform& a_transform
) {
    if (!a_destination.empty() && a_source && a_vertex < a_source->size()) {
        a_destination[a_vertex] = a_transform.ApplyTransformToDir((*a_source)[a_vertex]);
    }
}

void PlaceVertex(
    const SkinSource& a_source,
    const std::size_t a_vertex,
    const nifly::MatTransform& a_transform,
    SkinnedVertices& a_destination
) {
    a_destination.positions[a_vertex] = a_transform.ApplyTransform((*a_source.positions)[a_vertex]);
    PlaceDirection(a_source.normals, a_destination.normals, a_vertex, a_transform);
    PlaceDirection(a_source.tangents, a_destination.tangents, a_vertex, a_transform);
    PlaceDirection(a_source.bitangents, a_destination.bitangents, a_vertex, a_transform);
}
} // namespace

SkinSource SkinSourceOf(nifly::NifFile* const a_nifFile, nifly::NiShape* const a_shape) {
    return {
        .positions = a_nifFile->GetVertsForShape(a_shape),
        .normals = a_nifFile->GetNormalsForShape(a_shape),
        .tangents = a_nifFile->GetTangentsForShape(a_shape),
        .bitangents = a_nifFile->GetBitangentsForShape(a_shape),
    };
}

SkinnedVertices PlaceVertices(const SkinSource& a_source, const nifly::MatTransform& a_transform) {
    auto placed = ZeroedVertices(a_source);
    for (std::size_t vertex = 0; vertex < a_source.positions->size(); vertex++) {
        PlaceVertex(a_source, vertex, a_transform, placed);
    }
    NormalizeDirections(placed);
    return placed;
}

ShapeSkin::ShapeSkin(nifly::NifFile* const a_nifFile, nifly::NiShape* const a_shape)
    : _nifFile {a_nifFile}
    , _shape {a_shape}
    , _shapeBlock {a_nifFile->GetBlockID(a_shape)} {
    const auto& header = _nifFile->GetHeader();
    const auto* const skinInstance = dynamic_cast<nifly::NiSkinInstance*>(
        header.GetBlock<nifly::NiBoneContainer>(_shape->SkinInstanceRef())
    );
    auto* const partition = skinInstance ? header.GetBlock(skinInstance->skinPartitionRef) : nullptr;
    if (IsLegacyTriShape(_shape) && HasPartitionWeights(partition)) {
        _weightPartition = partition;
    }

    if (const auto* const positions = _nifFile->GetVertsForShape(_shape)) {
        _weighted.assign(positions->size(), 0);
    }
    ReadBones();
    if (_weightPartition != nullptr) {
        ReadPartitionInfluences();
    } else {
        ReadShapeInfluences();
    }
}

bool ShapeSkin::HasUsableBones() const {
    return std::ranges::any_of(_bones, &Bone::usable);
}

std::size_t ShapeSkin::UnweightedVertexCount() const {
    return static_cast<std::size_t>(std::ranges::count(_weighted, 0));
}

bool ShapeSkin::MovedBy(const std::span<const ObjectState> a_states) const {
    return a_states[_shapeBlock].posed || std::ranges::any_of(_bones, [&](const Bone& a_bone) {
        return a_bone.usable && a_states[a_bone.block].posed;
    });
}

SkinnedVertices ShapeSkin::AtRest(const SkinSource& a_source) const {
    return Deform(a_source, RestBoneTransforms(), GetShapeTransformToGlobal(_nifFile, _shape));
}

SkinnedVertices ShapeSkin::Posed(const SkinSource& a_source, const std::span<const ObjectState> a_states) const {
    return Deform(a_source, PosedBoneTransforms(a_states), a_states[_shapeBlock].world);
}

void ShapeSkin::ReadBones() {
    const auto& header = _nifFile->GetHeader();
    auto* const skinContainer = header.GetBlock<nifly::NiBoneContainer>(_shape->SkinInstanceRef());
    if (!skinContainer) {
        return;
    }

    // Older shapes without weights in a partition take them from skin data, which is laid out against the skeleton
    // root.
    const auto* const skinInstance = dynamic_cast<nifly::NiSkinInstance*>(skinContainer);
    const auto readsSkinData = IsLegacyTriShape(_shape) && _weightPartition == nullptr;
    _skeletonRoot = skinInstance ? skinInstance->targetRef.index : nifly::NIF_NPOS;
    const auto hasGlobalToSkin = _nifFile->GetShapeTransformGlobalToSkin(_shape, _globalToSkin);
    const auto hasSkeletonRoot = readsSkinData && hasGlobalToSkin && _skeletonRoot != nifly::NIF_NPOS;

    _bones.resize(skinContainer->boneRefs.GetSize());
    for (std::uint32_t index = 0; index < _bones.size(); index++) {
        auto& bone = _bones[index];
        bone.block = skinContainer->boneRefs.GetBlockRef(index);
        if (bone.block == nifly::NIF_NPOS || !_nifFile->GetShapeTransformSkinToBone(_shape, index, bone.skinToBone)) {
            continue;
        }
        auto* const node = header.GetBlock<nifly::NiNode>(bone.block);
        if (!node) {
            continue;
        }

        nifly::MatTransform toSkeletonRoot;
        bone.throughSkeletonRoot = hasSkeletonRoot
                                   && GetNodeTransformToAncestor(_nifFile, node, _skeletonRoot, toSkeletonRoot);
        bone.usable = true;
    }
}

void ShapeSkin::ReadPartitionInfluences() {
    for (const auto& partition : _weightPartition->partitions) {
        if (partition.numWeightsPerVertex == 0 || partition.bones.empty()) {
            continue;
        }

        const auto vertexLimit = PartitionVertexLimit(partition);
        for (std::size_t partitionVertex = 0; partitionVertex < vertexLimit; partitionVertex++) {
            ReadPartitionVertex(partition, partitionVertex);
        }
    }
}

void ShapeSkin::ReadPartitionVertex(
    const nifly::NiSkinPartition::PartitionBlock& a_partition,
    const std::size_t a_partitionVertex
) {
    const auto vertex = a_partition.hasVertexMap ? a_partition.vertexMap[a_partitionVertex]
                                                 : static_cast<std::uint16_t>(a_partitionVertex);
    // A vertex shared by partitions takes its weights from the first one.
    if (vertex >= _weighted.size() || _weighted[vertex] != 0) {
        return;
    }

    const auto weightCount = std::min<std::size_t>(a_partition.numWeightsPerVertex, 4);
    for (std::size_t slot = 0; slot < weightCount; slot++) {
        const auto localBone = PartitionBoneIndex(a_partition.boneIndices[a_partitionVertex], slot);
        if (localBone >= a_partition.bones.size()) {
            continue;
        }
        const auto bone = a_partition.bones[localBone];
        if (bone < _bones.size() && _bones[bone].usable) {
            AddInfluence(vertex, bone, PartitionWeight(a_partition.vertexWeights[a_partitionVertex], slot));
        }
    }
}

void ShapeSkin::ReadShapeInfluences() {
    for (std::uint32_t bone = 0; bone < _bones.size(); bone++) {
        if (!_bones[bone].usable) {
            continue;
        }

        std::unordered_map<std::uint16_t, float> weights;
        if (_nifFile->GetShapeBoneWeights(_shape, bone, weights) == 0) {
            continue;
        }
        for (const auto& [vertex, weight] : weights) {
            AddInfluence(vertex, bone, weight);
        }
    }
}

void ShapeSkin::AddInfluence(const std::size_t a_vertex, const std::uint32_t a_bone, const float a_weight) {
    if (a_vertex >= _weighted.size() || a_weight <= kMinSkinWeight) {
        return;
    }

    _influences.push_back({.vertex = static_cast<std::uint32_t>(a_vertex), .bone = a_bone, .weight = a_weight});
    _weighted[a_vertex] = 1;
}

std::vector<nifly::MatTransform> ShapeSkin::RestBoneTransforms() const {
    const auto& header = _nifFile->GetHeader();
    const auto shapeToGlobal = GetShapeTransformToGlobal(_nifFile, _shape);

    std::vector<nifly::MatTransform> transforms(_bones.size());
    for (std::size_t index = 0; index < _bones.size(); index++) {
        const auto& bone = _bones[index];
        if (!bone.usable) {
            continue;
        }

        auto* const node = header.GetBlock<nifly::NiNode>(bone.block);
        if (bone.throughSkeletonRoot) {
            nifly::MatTransform toSkeletonRoot;
            GetNodeTransformToAncestor(_nifFile, node, _skeletonRoot, toSkeletonRoot);
            transforms[index] = shapeToGlobal.ComposeTransforms(_globalToSkin)
                                    .ComposeTransforms(toSkeletonRoot)
                                    .ComposeTransforms(bone.skinToBone);
        } else {
            transforms[index] = GetObjectTransformToGlobal(_nifFile, node).ComposeTransforms(bone.skinToBone);
        }
    }
    return transforms;
}

std::vector<nifly::MatTransform> ShapeSkin::PosedBoneTransforms(const std::span<const ObjectState> a_states) const {
    std::vector<nifly::MatTransform> transforms(_bones.size());
    for (std::size_t index = 0; index < _bones.size(); index++) {
        const auto& bone = _bones[index];
        if (!bone.usable) {
            continue;
        }

        const auto& toGlobal = a_states[bone.block].world;
        if (bone.throughSkeletonRoot) {
            const auto toSkeletonRoot = a_states[_skeletonRoot].world.InverseTransform().ComposeTransforms(toGlobal);
            transforms[index] = a_states[_shapeBlock]
                                    .world.ComposeTransforms(_globalToSkin)
                                    .ComposeTransforms(toSkeletonRoot)
                                    .ComposeTransforms(bone.skinToBone);
        } else {
            transforms[index] = toGlobal.ComposeTransforms(bone.skinToBone);
        }
    }
    return transforms;
}

SkinnedVertices ShapeSkin::Deform(
    const SkinSource& a_source,
    const std::vector<nifly::MatTransform>& a_bones,
    const nifly::MatTransform& a_rigid
) const {
    auto deformed = ZeroedVertices(a_source);
    for (const auto& [vertex, bone, weight] : _influences) {
        const auto& transform = a_bones[bone];
        deformed.positions[vertex] += transform.ApplyTransform((*a_source.positions)[vertex]) * weight;
        AddDirection(a_source.normals, deformed.normals, vertex, transform, weight);
        AddDirection(a_source.tangents, deformed.tangents, vertex, transform, weight);
        AddDirection(a_source.bitangents, deformed.bitangents, vertex, transform, weight);
    }

    for (std::size_t vertex = 0; vertex < _weighted.size(); vertex++) {
        if (_weighted[vertex] == 0) {
            PlaceVertex(a_source, vertex, a_rigid, deformed);
        }
    }

    NormalizeDirections(deformed);
    return deformed;
}
