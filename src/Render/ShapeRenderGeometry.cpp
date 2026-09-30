#include "Render/ShapeRenderGeometry.h"
#include "Nif/BindPose.h"
#include "Nif/Transforms.h"

#include <QDebug>

#include <nifly/NifFile.hpp>
#include <nifly/Skin.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace {
constexpr float kMinSkinWeight = 0.000001F;

struct BoneTransform {
    nifly::MatTransform transform;
    bool valid = false;
};

struct SkinTarget {
    const ShapeRenderGeometry& source;
    ShapeRenderGeometry& skinned;
    std::vector<char>& weightedVertices;
};

const std::vector<nifly::Vector3>* VectorOrNull(const std::vector<nifly::Vector3>& a_values) {
    return a_values.empty() ? nullptr : &a_values;
}

std::string ShapeName(nifly::NiShape* a_shape) {
    const auto name = a_shape->name.get();
    return name.empty() ? std::string("<unnamed>") : name;
}

std::uint32_t BlockRefIndex(const nifly::NiBlockRef<nifly::NiNode>& a_ref) {
    return a_ref.index;
}

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

void NormalizeVectors(std::vector<nifly::Vector3>& a_values) {
    for (auto& value : a_values) {
        if (!value.IsZero(true)) {
            value.Normalize();
        }
    }
}

void TransformDirection(
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

bool ApplySkinWeight(
    const SkinTarget& a_target,
    const std::size_t a_vertex,
    const float a_weight,
    const nifly::MatTransform& a_transform
) {
    const auto& source = a_target.source;
    auto& skinned = a_target.skinned;
    if (!source.rawPositions || a_vertex >= source.rawPositions->size() || a_weight <= kMinSkinWeight) {
        return false;
    }

    skinned.skinnedPositions[a_vertex] += a_transform.ApplyTransform((*source.rawPositions)[a_vertex]) * a_weight;
    TransformDirection(source.rawNormals, skinned.skinnedNormals, a_vertex, a_transform, a_weight);
    TransformDirection(source.rawTangents, skinned.skinnedTangents, a_vertex, a_transform, a_weight);
    TransformDirection(source.rawBitangents, skinned.skinnedBitangents, a_vertex, a_transform, a_weight);
    a_target.weightedVertices[a_vertex] = 1;
    return true;
}

bool GetNodeTransformToGlobal(
    const nifly::NifFile* a_nifFile,
    const std::uint32_t a_nodeId,
    nifly::MatTransform& a_transform
) {
    auto* const node = a_nifFile->GetHeader().GetBlock<nifly::NiNode>(a_nodeId);
    if (!node) {
        return false;
    }

    a_transform = GetObjectTransformToGlobal(a_nifFile, node);
    return true;
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

std::vector<nifly::Triangle> GetShapeTriangles(const nifly::NiShape* a_shape, nifly::NiSkinPartition* a_skinPartition) {
    std::vector<nifly::Triangle> triangles;

    if (a_skinPartition) {
        a_skinPartition->PrepareTrueTriangles();
        for (const auto& partition : a_skinPartition->partitions) {
            triangles.insert(triangles.end(), partition.trueTriangles.begin(), partition.trueTriangles.end());
        }
        if (!triangles.empty()) {
            return triangles;
        }
    }

    a_shape->GetTriangles(triangles);
    return triangles;
}

std::vector<BoneTransform> GetBoneTransforms(
    nifly::NifFile* a_nifFile,
    nifly::NiShape* a_shape,
    const bool a_legacySkinDataMode
) {
    const auto& header = a_nifFile->GetHeader();
    auto* const skinContainer = header.GetBlock<nifly::NiBoneContainer>(a_shape->SkinInstanceRef());
    if (!skinContainer) {
        return {};
    }

    const auto shapeToGlobal = GetShapeTransformToGlobal(a_nifFile, a_shape);
    nifly::MatTransform globalToSkin;
    const auto hasGlobalToSkin = a_nifFile->GetShapeTransformGlobalToSkin(a_shape, globalToSkin);

    const auto* const skinInstance = dynamic_cast<nifly::NiSkinInstance*>(skinContainer);
    const auto skeletonRootId = skinInstance ? BlockRefIndex(skinInstance->targetRef) : nifly::NIF_NPOS;

    std::vector<BoneTransform> transforms(skinContainer->boneRefs.GetSize());
    for (std::uint32_t boneIndex = 0; boneIndex < skinContainer->boneRefs.GetSize(); boneIndex++) {
        const auto boneId = skinContainer->boneRefs.GetBlockRef(boneIndex);
        if (boneId == nifly::NIF_NPOS) {
            continue;
        }

        nifly::MatTransform skinToBone;
        if (!a_nifFile->GetShapeTransformSkinToBone(a_shape, boneIndex, skinToBone)) {
            continue;
        }

        auto* const bone = header.GetBlock<nifly::NiNode>(boneId);
        if (!bone) {
            continue;
        }

        if (a_legacySkinDataMode && hasGlobalToSkin && skeletonRootId != nifly::NIF_NPOS) {
            nifly::MatTransform boneToSkeletonRoot;
            if (GetNodeTransformToAncestor(a_nifFile, bone, skeletonRootId, boneToSkeletonRoot)) {
                transforms[boneIndex].transform = shapeToGlobal.ComposeTransforms(globalToSkin)
                                                      .ComposeTransforms(boneToSkeletonRoot)
                                                      .ComposeTransforms(skinToBone);
                transforms[boneIndex].valid = true;
                continue;
            }
        }

        nifly::MatTransform boneToGlobal;
        if (GetNodeTransformToGlobal(a_nifFile, boneId, boneToGlobal)) {
            transforms[boneIndex].transform = boneToGlobal.ComposeTransforms(skinToBone);
            transforms[boneIndex].valid = true;
        }
    }

    return transforms;
}

bool SkinPartitionVertex(
    const nifly::NiSkinPartition::PartitionBlock& a_partition,
    const std::size_t a_partitionVertex,
    const std::vector<BoneTransform>& a_boneTransforms,
    const SkinTarget& a_target
) {
    const auto shapeVertex = a_partition.hasVertexMap ? a_partition.vertexMap[a_partitionVertex]
                                                      : static_cast<std::uint16_t>(a_partitionVertex);
    if (shapeVertex >= a_target.weightedVertices.size() || a_target.weightedVertices[shapeVertex] != 0) {
        return false;
    }

    bool skinnedVertex = false;
    const auto weightCount = std::min<std::size_t>(a_partition.numWeightsPerVertex, 4);
    for (std::size_t weightSlot = 0; weightSlot < weightCount; weightSlot++) {
        const auto localBoneIndex = PartitionBoneIndex(a_partition.boneIndices[a_partitionVertex], weightSlot);
        if (localBoneIndex >= a_partition.bones.size()) {
            continue;
        }

        const auto boneIndex = a_partition.bones[localBoneIndex];
        if (boneIndex >= a_boneTransforms.size() || !a_boneTransforms[boneIndex].valid) {
            continue;
        }

        const auto weight = PartitionWeight(a_partition.vertexWeights[a_partitionVertex], weightSlot);
        skinnedVertex |= ApplySkinWeight(a_target, shapeVertex, weight, a_boneTransforms[boneIndex].transform);
    }

    return skinnedVertex;
}

bool SkinFromPartitions(
    const nifly::NiSkinPartition* a_skinPartition,
    const std::vector<BoneTransform>& a_boneTransforms,
    const SkinTarget& a_target
) {
    bool skinnedAnyVertex = false;

    for (const auto& partition : a_skinPartition->partitions) {
        if (partition.numWeightsPerVertex == 0 || partition.bones.empty()) {
            continue;
        }

        const auto vertexLimit = PartitionVertexLimit(partition);
        for (std::size_t partitionVertex = 0; partitionVertex < vertexLimit; partitionVertex++) {
            skinnedAnyVertex |= SkinPartitionVertex(partition, partitionVertex, a_boneTransforms, a_target);
        }
    }

    return skinnedAnyVertex;
}

bool SkinFromBoneWeights(
    const nifly::NifFile* a_nifFile,
    nifly::NiShape* a_shape,
    const std::vector<BoneTransform>& a_boneTransforms,
    const SkinTarget& a_target
) {
    bool skinnedAnyVertex = false;

    for (std::uint32_t boneIndex = 0; boneIndex < a_boneTransforms.size(); boneIndex++) {
        if (!a_boneTransforms[boneIndex].valid) {
            continue;
        }

        std::unordered_map<std::uint16_t, float> weights;
        if (a_nifFile->GetShapeBoneWeights(a_shape, boneIndex, weights) == 0) {
            continue;
        }

        for (const auto& [vertex, weight] : weights) {
            skinnedAnyVertex |= ApplySkinWeight(a_target, vertex, weight, a_boneTransforms[boneIndex].transform);
        }
    }

    return skinnedAnyVertex;
}

void CopyRigidVertex(
    const ShapeRenderGeometry& a_source,
    const std::size_t a_vertex,
    const nifly::MatTransform& a_transform,
    ShapeRenderGeometry& a_destination
) {
    a_destination.skinnedPositions[a_vertex] = a_transform.ApplyTransform((*a_source.rawPositions)[a_vertex]);

    if (!a_destination.skinnedNormals.empty() && a_source.rawNormals && a_vertex < a_source.rawNormals->size()) {
        a_destination.skinnedNormals[a_vertex] = a_transform.ApplyTransformToDir((*a_source.rawNormals)[a_vertex]);
    }
    if (!a_destination.skinnedTangents.empty() && a_source.rawTangents && a_vertex < a_source.rawTangents->size()) {
        a_destination.skinnedTangents[a_vertex] = a_transform.ApplyTransformToDir((*a_source.rawTangents)[a_vertex]);
    }
    if (!a_destination.skinnedBitangents.empty()
        && a_source.rawBitangents
        && a_vertex < a_source.rawBitangents->size()) {
        a_destination.skinnedBitangents[a_vertex] = a_transform.ApplyTransformToDir(
            (*a_source.rawBitangents)[a_vertex]
        );
    }
}

ShapeRenderGeometry MakeSkinnedGeometry(const ShapeRenderGeometry& a_geometry) {
    ShapeRenderGeometry skinnedGeometry = a_geometry;
    skinnedGeometry.skinnedPositions.assign(a_geometry.rawPositions->size(), {});
    if (HasSameSize(a_geometry.rawNormals, a_geometry.rawPositions)) {
        skinnedGeometry.skinnedNormals.assign(a_geometry.rawPositions->size(), {});
    }
    if (HasSameSize(a_geometry.rawTangents, a_geometry.rawPositions)) {
        skinnedGeometry.skinnedTangents.assign(a_geometry.rawPositions->size(), {});
    }
    if (HasSameSize(a_geometry.rawBitangents, a_geometry.rawPositions)) {
        skinnedGeometry.skinnedBitangents.assign(a_geometry.rawPositions->size(), {});
    }
    return skinnedGeometry;
}

void CopyRigidVertices(const SkinTarget& a_target, const nifly::NifFile* a_nifFile, nifly::NiShape* a_shape) {
    const auto rigidVertices = static_cast<std::size_t>(std::ranges::count(a_target.weightedVertices, 0));
    if (rigidVertices == 0) {
        return;
    }

    qWarning("NIF shape '%s' has %zu vertices without usable skin weights", ShapeName(a_shape).c_str(), rigidVertices);
    const auto rigidTransform = GetShapeTransformToGlobal(a_nifFile, a_shape);
    for (std::size_t vertex = 0; vertex < a_target.weightedVertices.size(); vertex++) {
        if (a_target.weightedVertices[vertex] == 0) {
            CopyRigidVertex(a_target.source, vertex, rigidTransform, a_target.skinned);
        }
    }
}

ShapeRenderGeometry PlaceSkinnedGeometry(
    const ShapeRenderGeometry& a_geometry,
    const nifly::MatTransform& a_skinToModel
) {
    auto placed = MakeSkinnedGeometry(a_geometry);
    for (std::size_t vertex = 0; vertex < a_geometry.rawPositions->size(); vertex++) {
        CopyRigidVertex(a_geometry, vertex, a_skinToModel, placed);
    }

    NormalizeVectors(placed.skinnedNormals);
    NormalizeVectors(placed.skinnedTangents);
    NormalizeVectors(placed.skinnedBitangents);

    placed.skinned = true;
    placed.bounds = nifly::BoundingSphere(placed.skinnedPositions);
    placed.modelTransform.Clear();
    return placed;
}

bool TryBuildSkinnedGeometry(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape, ShapeRenderGeometry& a_geometry) {
    if (!a_shape->HasSkinInstance() || !a_geometry.rawPositions || a_geometry.rawPositions->empty()) {
        return false;
    }

    const auto& header = a_nifFile->GetHeader();
    auto* const skinContainer = header.GetBlock<nifly::NiBoneContainer>(a_shape->SkinInstanceRef());
    if (!skinContainer) {
        qWarning("Skipping skinning for NIF shape '%s': missing skin instance", ShapeName(a_shape).c_str());
        return false;
    }

    const auto* const skinInstance = dynamic_cast<nifly::NiSkinInstance*>(skinContainer);
    // Fallout 4 skin data stores no skin-to-model transform, and FaceGen bone nodes carry positions without
    // rotations, so skinning with them tears FaceGen shapes. The fitted bind pose places the shape as one piece.
    if (!skinInstance) {
        if (const auto skinToModel = FitSkinToModel(a_nifFile, a_shape)) {
            a_geometry = PlaceSkinnedGeometry(a_geometry, *skinToModel);
            return true;
        }
    }

    auto* skinPartition = skinInstance ? header.GetBlock(skinInstance->skinPartitionRef) : nullptr;

    const bool useLegacyPartition = IsLegacyTriShape(a_shape) && HasPartitionWeights(skinPartition);
    auto boneTransforms = GetBoneTransforms(a_nifFile, a_shape, IsLegacyTriShape(a_shape) && !useLegacyPartition);
    if (boneTransforms.empty()
        || std::ranges::none_of(boneTransforms, [](const BoneTransform& a_transform) { return a_transform.valid; })) {
        qWarning("Skipping skinning for NIF shape '%s': missing bone transforms", ShapeName(a_shape).c_str());
        return false;
    }

    auto skinnedGeometry = MakeSkinnedGeometry(a_geometry);
    std::vector<char> weightedVertices(a_geometry.rawPositions->size(), 0);
    const SkinTarget target {.source = a_geometry, .skinned = skinnedGeometry, .weightedVertices = weightedVertices};
    const auto skinnedAnyVertex = useLegacyPartition ? SkinFromPartitions(skinPartition, boneTransforms, target)
                                                     : SkinFromBoneWeights(a_nifFile, a_shape, boneTransforms, target);
    if (!skinnedAnyVertex) {
        qWarning("Skipping skinning for NIF shape '%s': no usable skin weights", ShapeName(a_shape).c_str());
        return false;
    }

    CopyRigidVertices(target, a_nifFile, a_shape);

    NormalizeVectors(skinnedGeometry.skinnedNormals);
    NormalizeVectors(skinnedGeometry.skinnedTangents);
    NormalizeVectors(skinnedGeometry.skinnedBitangents);

    skinnedGeometry.skinned = true;
    skinnedGeometry.bounds = nifly::BoundingSphere(skinnedGeometry.skinnedPositions);
    skinnedGeometry.triangles = GetShapeTriangles(a_shape, useLegacyPartition ? skinPartition : nullptr);
    skinnedGeometry.modelTransform.Clear();

    a_geometry = std::move(skinnedGeometry);
    return true;
}
} // namespace

const std::vector<nifly::Vector3>* ShapeRenderGeometry::Positions() const {
    return skinned ? &skinnedPositions : rawPositions;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::Normals() const {
    return skinned ? VectorOrNull(skinnedNormals) : rawNormals;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::Tangents() const {
    return skinned ? VectorOrNull(skinnedTangents) : rawTangents;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::Bitangents() const {
    return skinned ? VectorOrNull(skinnedBitangents) : rawBitangents;
}

void ValidateShapeGeometry(nifly::NiShape* a_shape) {
    if (auto* const geomData = a_shape->GetGeomData()) {
        if (!a_shape->HasUVs()) {
            a_shape->SetUVs(true);
        }
        if (!a_shape->HasNormals()) {
            a_shape->SetNormals(true);
            geomData->RecalcNormals();
        }
        if (!a_shape->HasTangents() || geomData->tangents.empty()) {
            a_shape->SetTangents(true);
            geomData->CalcTangentSpace();
        }
        if (!a_shape->HasVertexColors()) {
            a_shape->SetVertexColors(true);
        }
    }
}

ShapeRenderGeometry PrepareShapeRenderGeometry(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape) {
    ShapeRenderGeometry geometry;
    geometry.rawPositions = a_nifFile->GetVertsForShape(a_shape);
    geometry.rawNormals = a_nifFile->GetNormalsForShape(a_shape);
    geometry.rawTangents = a_nifFile->GetTangentsForShape(a_shape);
    geometry.rawBitangents = a_nifFile->GetBitangentsForShape(a_shape);
    geometry.modelTransform = GetShapeTransformToGlobal(a_nifFile, a_shape);
    geometry.bounds = GetBoundingSphere(a_nifFile, a_shape);
    geometry.triangles = GetShapeTriangles(a_shape, nullptr);

    TryBuildSkinnedGeometry(a_nifFile, a_shape, geometry);
    return geometry;
}
