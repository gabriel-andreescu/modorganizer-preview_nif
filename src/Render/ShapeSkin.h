#pragma once

#include <nifly/Object3d.hpp>
#include <nifly/Skin.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace nifly {
class NifFile;
class NiShape;
}

struct ObjectState;

// A skinned shape's vertex data as the NIF stores it, before its bones move it. All but the positions may be null.
struct SkinSource {
    const std::vector<nifly::Vector3>* positions = nullptr;
    const std::vector<nifly::Vector3>* normals = nullptr;
    const std::vector<nifly::Vector3>* tangents = nullptr;
    const std::vector<nifly::Vector3>* bitangents = nullptr;
};

// The same data where the bones put it. A list the source lacks, or has in another size, stays empty.
struct SkinnedVertices {
    std::vector<nifly::Vector3> positions;
    std::vector<nifly::Vector3> normals;
    std::vector<nifly::Vector3> tangents;
    std::vector<nifly::Vector3> bitangents;
};

[[nodiscard]] SkinSource SkinSourceOf(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape);
// Every vertex moved by one transform.
[[nodiscard]] SkinnedVertices PlaceVertices(const SkinSource& a_source, const nifly::MatTransform& a_transform);

// A skinned shape's bones and the pull each has on each vertex, to work out where bone transforms put the vertices.
class ShapeSkin {
public:
    // The shape must have a skin instance.
    ShapeSkin(nifly::NifFile* a_nifFile, nifly::NiShape* a_shape);

    [[nodiscard]] bool HasUsableBones() const;
    // Whether any bone pulls any vertex. A skin without a pull places nothing.
    [[nodiscard]] bool HasInfluences() const noexcept {
        return !_influences.empty();
    }
    [[nodiscard]] std::size_t UnweightedVertexCount() const;
    // The partition the weights were read from, which then also holds the triangles. Null when the shape's own
    // weights were used.
    [[nodiscard]] nifly::NiSkinPartition* WeightPartition() const noexcept {
        return _weightPartition;
    }

    // Whether a pose moves the shape or one of its bones.
    [[nodiscard]] bool MovedBy(std::span<const ObjectState> a_states) const;
    // The vertices with the bones where the NIF stores their nodes.
    [[nodiscard]] SkinnedVertices AtRest(const SkinSource& a_source) const;
    // The vertices with the bones where a pose has their nodes.
    [[nodiscard]] SkinnedVertices Posed(const SkinSource& a_source, std::span<const ObjectState> a_states) const;

private:
    struct Bone {
        std::uint32_t block = 0;
        nifly::MatTransform skinToBone;
        // Older skin data places a bone through the skeleton root instead of the scene.
        bool throughSkeletonRoot = false;
        // False for a bone without a node or a skin transform, which pulls nothing.
        bool usable = false;
    };

    // One bone's pull on one vertex.
    struct Influence {
        std::uint32_t vertex = 0;
        std::uint32_t bone = 0;
        float weight = 0.0F;
    };

    void ReadBones();
    void ReadPartitionInfluences();
    void ReadPartitionVertex(const nifly::NiSkinPartition::PartitionBlock& a_partition, std::size_t a_partitionVertex);
    void ReadShapeInfluences();
    void AddInfluence(std::size_t a_vertex, std::uint32_t a_bone, float a_weight);

    [[nodiscard]] std::vector<nifly::MatTransform> RestBoneTransforms() const;
    [[nodiscard]] std::vector<nifly::MatTransform> PosedBoneTransforms(std::span<const ObjectState> a_states) const;
    // Vertices no bone pulls go where a_rigid puts the shape.
    [[nodiscard]] SkinnedVertices Deform(
        const SkinSource& a_source,
        const std::vector<nifly::MatTransform>& a_bones,
        const nifly::MatTransform& a_rigid
    ) const;

    nifly::NifFile* _nifFile = nullptr;
    nifly::NiShape* _shape = nullptr;
    std::uint32_t _shapeBlock = 0;
    nifly::NiSkinPartition* _weightPartition = nullptr;
    // Used by the bones that are placed through the skeleton root.
    nifly::MatTransform _globalToSkin;
    std::uint32_t _skeletonRoot = 0;
    std::vector<Bone> _bones;
    // In the order the pulls add up, which the sums depend on to the last bit.
    std::vector<Influence> _influences;
    std::vector<char> _weighted;
};
