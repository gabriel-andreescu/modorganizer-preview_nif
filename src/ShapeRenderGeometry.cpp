#include "ShapeRenderGeometry.h"
#include "NifTransforms.h"

#include <QDebug>

#include <NifFile.hpp>
#include <Skin.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace {
constexpr float MinSkinWeight = 0.000001f;

struct BoneTransform {
    nifly::MatTransform transform;
    bool valid = false;
};

const std::vector<nifly::Vector3>* vectorOrNull(const std::vector<nifly::Vector3>& values) {
    return values.empty() ? nullptr : &values;
}

std::string shapeName(nifly::NiShape* shape) {
    const auto name = shape->name.get();
    return name.empty() ? std::string("<unnamed>") : name;
}

std::uint32_t blockRefIndex(const nifly::NiBlockRef<nifly::NiNode>& ref) {
    return ref.index;
}

bool isLegacyTriShape(const nifly::NiShape* shape) {
    return shape->HasType<nifly::NiTriShape>() || shape->HasType<nifly::NiTriStrips>();
}

bool hasSameSize(const std::vector<nifly::Vector3>* values, const std::vector<nifly::Vector3>* positions) {
    return values && positions && values->size() == positions->size();
}

float partitionWeight(const nifly::VertexWeight& weights, const std::size_t slot) {
    switch (slot) {
        case 0:  return weights.w1;
        case 1:  return weights.w2;
        case 2:  return weights.w3;
        case 3:  return weights.w4;
        default: return 0.0f;
    }
}

std::uint8_t partitionBoneIndex(const nifly::BoneIndices& indices, const std::size_t slot) {
    switch (slot) {
        case 0:  return indices.i1;
        case 1:  return indices.i2;
        case 2:  return indices.i3;
        case 3:  return indices.i4;
        default: return 0;
    }
}

void normalizeVectors(std::vector<nifly::Vector3>& values) {
    for (auto& value : values) {
        if (!value.IsZero(true)) {
            value.Normalize();
        }
    }
}

void transformDirection(
    const std::vector<nifly::Vector3>* source,
    std::vector<nifly::Vector3>& destination,
    const std::size_t vertex,
    const nifly::MatTransform& transform,
    const float weight
) {
    if (source && vertex < source->size() && vertex < destination.size()) {
        destination[vertex] += transform.ApplyTransformToDir((*source)[vertex]) * weight;
    }
}

bool applySkinWeight(
    const ShapeRenderGeometry& geometry,
    const std::size_t vertex,
    const float weight,
    const nifly::MatTransform& transform,
    ShapeRenderGeometry& skinnedGeometry,
    std::vector<char>& weightedVertices
) {
    if (!geometry.rawPositions || vertex >= geometry.rawPositions->size() || weight <= MinSkinWeight) {
        return false;
    }

    skinnedGeometry.skinnedPositions[vertex] += transform.ApplyTransform((*geometry.rawPositions)[vertex]) * weight;
    transformDirection(geometry.rawNormals, skinnedGeometry.skinnedNormals, vertex, transform, weight);
    transformDirection(geometry.rawTangents, skinnedGeometry.skinnedTangents, vertex, transform, weight);
    transformDirection(geometry.rawBitangents, skinnedGeometry.skinnedBitangents, vertex, transform, weight);
    weightedVertices[vertex] = true;
    return true;
}

bool getNodeTransformToGlobal(
    const nifly::NifFile* nifFile,
    const std::uint32_t nodeId,
    nifly::MatTransform& transform
) {
    auto* const node = nifFile->GetHeader().GetBlock<nifly::NiNode>(nodeId);
    if (!node) {
        return false;
    }

    transform = GetObjectTransformToGlobal(nifFile, node);
    return true;
}

bool hasPartitionWeights(const nifly::NiSkinPartition* skinPartition) {
    if (!skinPartition) {
        return false;
    }

    return std::ranges::any_of(skinPartition->partitions, [](const nifly::NiSkinPartition::PartitionBlock& partition) {
        return partition.numWeightsPerVertex
               > 0
               && !partition.bones.empty()
               && !partition.vertexWeights.empty()
               && !partition.boneIndices.empty();
    });
}

std::vector<nifly::BoneIndices>::size_type partitionVertexLimit(
    const nifly::NiSkinPartition::PartitionBlock& partition
) {
    auto limit = partition.boneIndices.size();
    limit = std::min(limit, partition.vertexWeights.size());
    if (partition.hasVertexMap) {
        limit = std::min(limit, partition.vertexMap.size());
    }
    if (partition.numVertices > 0) {
        limit = std::min(limit, static_cast<std::size_t>(partition.numVertices));
    }
    return limit;
}

std::vector<nifly::Triangle> getShapeTriangles(const nifly::NiShape* shape, nifly::NiSkinPartition* skinPartition) {
    std::vector<nifly::Triangle> triangles;

    if (skinPartition) {
        skinPartition->PrepareTrueTriangles();
        for (const auto& partition : skinPartition->partitions) {
            triangles.insert(triangles.end(), partition.trueTriangles.begin(), partition.trueTriangles.end());
        }
        if (!triangles.empty()) {
            return triangles;
        }
    }

    shape->GetTriangles(triangles);
    return triangles;
}

std::vector<BoneTransform> getBoneTransforms(
    nifly::NifFile* nifFile,
    nifly::NiShape* shape,
    const bool legacySkinDataMode
) {
    const auto& header = nifFile->GetHeader();
    auto* const skinContainer = header.GetBlock<nifly::NiBoneContainer>(shape->SkinInstanceRef());
    if (!skinContainer) {
        return {};
    }

    const auto shapeToGlobal = GetShapeTransformToGlobal(nifFile, shape);
    nifly::MatTransform globalToSkin;
    const auto hasGlobalToSkin = nifFile->GetShapeTransformGlobalToSkin(shape, globalToSkin);

    auto* const skinInstance = dynamic_cast<nifly::NiSkinInstance*>(skinContainer);
    const auto skeletonRootId = skinInstance ? blockRefIndex(skinInstance->targetRef) : nifly::NIF_NPOS;

    std::vector<BoneTransform> transforms(skinContainer->boneRefs.GetSize());
    for (std::uint32_t boneIndex = 0; boneIndex < skinContainer->boneRefs.GetSize(); boneIndex++) {
        const auto boneId = skinContainer->boneRefs.GetBlockRef(boneIndex);
        if (boneId == nifly::NIF_NPOS) {
            continue;
        }

        nifly::MatTransform skinToBone;
        if (!nifFile->GetShapeTransformSkinToBone(shape, boneIndex, skinToBone)) {
            continue;
        }

        auto* const bone = header.GetBlock<nifly::NiNode>(boneId);
        if (!bone) {
            continue;
        }

        if (legacySkinDataMode && hasGlobalToSkin && skeletonRootId != nifly::NIF_NPOS) {
            nifly::MatTransform boneToSkeletonRoot;
            if (GetNodeTransformToAncestor(nifFile, bone, skeletonRootId, boneToSkeletonRoot)) {
                transforms[boneIndex].transform = shapeToGlobal.ComposeTransforms(globalToSkin)
                                                      .ComposeTransforms(boneToSkeletonRoot)
                                                      .ComposeTransforms(skinToBone);
                transforms[boneIndex].valid = true;
                continue;
            }
        }

        nifly::MatTransform boneToGlobal;
        if (getNodeTransformToGlobal(nifFile, boneId, boneToGlobal)) {
            transforms[boneIndex].transform = boneToGlobal.ComposeTransforms(skinToBone);
            transforms[boneIndex].valid = true;
        }
    }

    return transforms;
}

bool skinFromPartitions(
    const nifly::NiSkinPartition* skinPartition,
    const std::vector<BoneTransform>& boneTransforms,
    const ShapeRenderGeometry& geometry,
    ShapeRenderGeometry& skinnedGeometry,
    std::vector<char>& weightedVertices
) {
    bool skinnedAnyVertex = false;

    for (const auto& partition : skinPartition->partitions) {
        const auto weightCount = std::min<std::size_t>(partition.numWeightsPerVertex, 4);
        if (weightCount == 0 || partition.bones.empty()) {
            continue;
        }

        const auto vertexLimit = partitionVertexLimit(partition);
        for (std::size_t partitionVertex = 0; partitionVertex < vertexLimit; partitionVertex++) {
            const auto shapeVertex = partition.hasVertexMap ? partition.vertexMap[partitionVertex]
                                                            : static_cast<std::uint16_t>(partitionVertex);
            if (shapeVertex >= weightedVertices.size() || weightedVertices[shapeVertex]) {
                continue;
            }

            bool skinnedVertex = false;
            for (std::size_t weightSlot = 0; weightSlot < weightCount; weightSlot++) {
                const auto localBoneIndex = partitionBoneIndex(partition.boneIndices[partitionVertex], weightSlot);
                if (localBoneIndex >= partition.bones.size()) {
                    continue;
                }

                const auto boneIndex = partition.bones[localBoneIndex];
                if (boneIndex >= boneTransforms.size() || !boneTransforms[boneIndex].valid) {
                    continue;
                }

                const auto weight = partitionWeight(partition.vertexWeights[partitionVertex], weightSlot);
                skinnedVertex |= applySkinWeight(
                    geometry,
                    shapeVertex,
                    weight,
                    boneTransforms[boneIndex].transform,
                    skinnedGeometry,
                    weightedVertices
                );
            }

            skinnedAnyVertex |= skinnedVertex;
        }
    }

    return skinnedAnyVertex;
}

bool skinFromBoneWeights(
    const nifly::NifFile* nifFile,
    nifly::NiShape* shape,
    const std::vector<BoneTransform>& boneTransforms,
    const ShapeRenderGeometry& geometry,
    ShapeRenderGeometry& skinnedGeometry,
    std::vector<char>& weightedVertices
) {
    bool skinnedAnyVertex = false;

    for (std::uint32_t boneIndex = 0; boneIndex < boneTransforms.size(); boneIndex++) {
        if (!boneTransforms[boneIndex].valid) {
            continue;
        }

        std::unordered_map<std::uint16_t, float> weights;
        if (nifFile->GetShapeBoneWeights(shape, boneIndex, weights) == 0) {
            continue;
        }

        for (const auto& [vertex, weight] : weights) {
            skinnedAnyVertex |= applySkinWeight(
                geometry,
                vertex,
                weight,
                boneTransforms[boneIndex].transform,
                skinnedGeometry,
                weightedVertices
            );
        }
    }

    return skinnedAnyVertex;
}

void copyRigidVertex(
    const ShapeRenderGeometry& source,
    const std::size_t vertex,
    const nifly::MatTransform& transform,
    ShapeRenderGeometry& destination
) {
    destination.skinnedPositions[vertex] = transform.ApplyTransform((*source.rawPositions)[vertex]);

    if (!destination.skinnedNormals.empty() && source.rawNormals && vertex < source.rawNormals->size()) {
        destination.skinnedNormals[vertex] = transform.ApplyTransformToDir((*source.rawNormals)[vertex]);
    }
    if (!destination.skinnedTangents.empty() && source.rawTangents && vertex < source.rawTangents->size()) {
        destination.skinnedTangents[vertex] = transform.ApplyTransformToDir((*source.rawTangents)[vertex]);
    }
    if (!destination.skinnedBitangents.empty() && source.rawBitangents && vertex < source.rawBitangents->size()) {
        destination.skinnedBitangents[vertex] = transform.ApplyTransformToDir((*source.rawBitangents)[vertex]);
    }
}

bool tryBuildSkinnedGeometry(nifly::NifFile* nifFile, nifly::NiShape* shape, ShapeRenderGeometry& geometry) {
    if (!shape->HasSkinInstance() || !geometry.rawPositions || geometry.rawPositions->empty()) {
        return false;
    }

    const auto& header = nifFile->GetHeader();
    auto* const skinContainer = header.GetBlock<nifly::NiBoneContainer>(shape->SkinInstanceRef());
    if (!skinContainer) {
        qWarning("Skipping skinning for NIF shape '%s': missing skin instance", shapeName(shape).c_str());
        return false;
    }

    auto* const skinInstance = dynamic_cast<nifly::NiSkinInstance*>(skinContainer);
    auto* skinPartition = skinInstance ? header.GetBlock(skinInstance->skinPartitionRef) : nullptr;

    const bool useLegacyPartition = isLegacyTriShape(shape) && hasPartitionWeights(skinPartition);
    auto boneTransforms = getBoneTransforms(nifFile, shape, isLegacyTriShape(shape) && !useLegacyPartition);
    if (boneTransforms.empty() || std::ranges::none_of(boneTransforms, [](const BoneTransform& transform) {
            return transform.valid;
        })) {
        qWarning("Skipping skinning for NIF shape '%s': missing bone transforms", shapeName(shape).c_str());
        return false;
    }

    ShapeRenderGeometry skinnedGeometry = geometry;
    skinnedGeometry.skinnedPositions.assign(geometry.rawPositions->size(), {});
    if (hasSameSize(geometry.rawNormals, geometry.rawPositions)) {
        skinnedGeometry.skinnedNormals.assign(geometry.rawPositions->size(), {});
    }
    if (hasSameSize(geometry.rawTangents, geometry.rawPositions)) {
        skinnedGeometry.skinnedTangents.assign(geometry.rawPositions->size(), {});
    }
    if (hasSameSize(geometry.rawBitangents, geometry.rawPositions)) {
        skinnedGeometry.skinnedBitangents.assign(geometry.rawPositions->size(), {});
    }

    std::vector<char> weightedVertices(geometry.rawPositions->size(), false);
    const auto skinnedAnyVertex = useLegacyPartition ? skinFromPartitions(
                                                           skinPartition,
                                                           boneTransforms,
                                                           geometry,
                                                           skinnedGeometry,
                                                           weightedVertices
                                                       )
                                                     : skinFromBoneWeights(
                                                           nifFile,
                                                           shape,
                                                           boneTransforms,
                                                           geometry,
                                                           skinnedGeometry,
                                                           weightedVertices
                                                       );

    if (!skinnedAnyVertex) {
        qWarning("Skipping skinning for NIF shape '%s': no usable skin weights", shapeName(shape).c_str());
        return false;
    }

    const auto rigidTransform = GetShapeTransformToGlobal(nifFile, shape);
    const auto rigidVertices = static_cast<std::size_t>(std::ranges::count(weightedVertices, 0));
    if (rigidVertices > 0) {
        qWarning(
            "NIF shape '%s' has %zu vertices without usable skin weights",
            shapeName(shape).c_str(),
            rigidVertices
        );
    }
    for (std::size_t vertex = 0; vertex < weightedVertices.size(); vertex++) {
        if (!weightedVertices[vertex]) {
            copyRigidVertex(geometry, vertex, rigidTransform, skinnedGeometry);
        }
    }

    normalizeVectors(skinnedGeometry.skinnedNormals);
    normalizeVectors(skinnedGeometry.skinnedTangents);
    normalizeVectors(skinnedGeometry.skinnedBitangents);

    skinnedGeometry.skinned = true;
    skinnedGeometry.bounds = nifly::BoundingSphere(skinnedGeometry.skinnedPositions);
    skinnedGeometry.triangles = getShapeTriangles(shape, useLegacyPartition ? skinPartition : nullptr);
    skinnedGeometry.modelTransform.Clear();

    geometry = std::move(skinnedGeometry);
    return true;
}
} // namespace

const std::vector<nifly::Vector3>* ShapeRenderGeometry::positions() const {
    return skinned ? &skinnedPositions : rawPositions;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::normals() const {
    return skinned ? vectorOrNull(skinnedNormals) : rawNormals;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::tangents() const {
    return skinned ? vectorOrNull(skinnedTangents) : rawTangents;
}

const std::vector<nifly::Vector3>* ShapeRenderGeometry::bitangents() const {
    return skinned ? vectorOrNull(skinnedBitangents) : rawBitangents;
}

void validateShapeGeometry(nifly::NiShape* shape) {
    if (auto* const geomData = shape->GetGeomData()) {
        if (!shape->HasUVs()) {
            shape->SetUVs(true);
        }
        if (!shape->HasNormals()) {
            shape->SetNormals(true);
            geomData->RecalcNormals();
        }
        if (!shape->HasTangents() || geomData->tangents.empty()) {
            shape->SetTangents(true);
            geomData->CalcTangentSpace();
        }
        if (!shape->HasVertexColors()) {
            shape->SetVertexColors(true);
        }
    }
}

ShapeRenderGeometry prepareShapeRenderGeometry(nifly::NifFile* nifFile, nifly::NiShape* shape) {
    ShapeRenderGeometry geometry;
    geometry.rawPositions = nifFile->GetVertsForShape(shape);
    geometry.rawNormals = nifFile->GetNormalsForShape(shape);
    geometry.rawTangents = nifFile->GetTangentsForShape(shape);
    geometry.rawBitangents = nifFile->GetBitangentsForShape(shape);
    geometry.modelTransform = GetShapeTransformToGlobal(nifFile, shape);
    geometry.bounds = GetBoundingSphere(nifFile, shape);
    geometry.triangles = getShapeTriangles(shape, nullptr);

    tryBuildSkinnedGeometry(nifFile, shape, geometry);
    return geometry;
}
