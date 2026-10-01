#include "Nif/Havok/CompressedMesh.h"

#include "Nif/Havok/Packfile.h"
#include "Nif/Havok/Transform.h"

#include <QString>

#include <array>
#include <bit>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Havok {
namespace {
    constexpr std::uint32_t kShapeConvexRadius = 0x14;
    constexpr std::uint32_t kShapeData = 0x60;

    constexpr std::uint32_t kTree = 0x10;
    constexpr std::uint32_t kDomainMin = kTree + 0x10;
    constexpr std::uint32_t kDomainMax = kTree + 0x20;
    constexpr std::uint32_t kSections = kTree + 0x40;
    constexpr std::uint32_t kPrimitives = kTree + 0x50;
    constexpr std::uint32_t kSharedVerticesIndex = kTree + 0x60;
    constexpr std::uint32_t kPackedVertices = kTree + 0x70;
    constexpr std::uint32_t kSharedVertices = kTree + 0x80;

    constexpr std::uint32_t kSectionSize = 0x60;
    constexpr std::uint32_t kSectionNodes = 0x00;
    constexpr std::uint32_t kSectionDomainMin = 0x10;
    constexpr std::uint32_t kSectionDomainMax = 0x20;
    constexpr std::uint32_t kSectionCodec = 0x30;
    constexpr std::uint32_t kSectionFirstPackedVertex = 0x48;
    constexpr std::uint32_t kSectionSharedVertices = 0x4C;
    constexpr std::uint32_t kSectionPrimitives = 0x50;
    constexpr std::uint32_t kSectionPackedVertexCount = 0x58;
    constexpr std::uint32_t kSectionPage = 0x5C;

    constexpr std::uint32_t kPrimitiveSize = 4;
    constexpr std::uint32_t kNodeSize = 4;
    constexpr std::uint32_t kPackedVertexSize = 4;
    constexpr std::uint32_t kSharedVertexSize = 8;
    constexpr std::uint32_t kSlotSize = 2;

    // The game asserts on this filler primitive instead of colliding with it.
    constexpr std::array<std::uint8_t, 4> kDeadPrimitive {0xDE, 0xAD, 0xDE, 0xAD};

    // Custom primitive header bits: the shape kind, the vertex compression method and an explicit radius.
    constexpr std::uint32_t kCustomKindMask = 0x0F;
    constexpr std::uint32_t kCustomRadiusMask = 0xC0;
    constexpr std::uint32_t kCustomSphere = 0;
    constexpr std::uint32_t kCustomCapsule = 1;
    constexpr std::uint32_t kCustomConvex = 2;

    struct Box {
        glm::vec3 min {};
        glm::vec3 max {};

        [[nodiscard]] glm::vec3 Point(const glm::vec3& a_fraction) const {
            return min + ((max - min) * a_fraction);
        }
    };

    struct Section {
        std::uint32_t offset = 0;
        std::array<float, 6> codec {};
        std::uint32_t firstPackedVertex = 0;
        std::uint32_t packedVertexCount = 0;
        // Index slot of vertex 0. Vertices past the packed ones and custom primitive data live in this slot range.
        std::int64_t firstSlot = 0;
        std::uint32_t firstSharedVertex = 0;
        Packfile::Range nodes;
        Box domain;
    };

    std::uint32_t Element(const Packfile::Range& a_array, const std::int64_t a_index, const std::uint32_t a_size) {
        if (a_index < 0 || std::cmp_greater_equal(a_index, a_array.count)) {
            throw std::runtime_error(QStringLiteral("Havok compressed mesh index %1 is outside its %2-element array")
                    .arg(a_index)
                    .arg(a_array.count)
                    .toStdString());
        }
        return a_array.offset + static_cast<std::uint32_t>(a_index * a_size);
    }

    // Tree nodes shrink their parent's box by a squared nibble per side, in steps of 1/226 of its size.
    Box ChildBox(const Box& a_parent, const std::array<std::uint8_t, 3>& a_code) {
        const auto step = (a_parent.max - a_parent.min) / 226.0F;
        Box child = a_parent;
        for (glm::length_t axis = 0; axis < 3; axis++) {
            const auto minSteps = static_cast<float>(a_code.at(static_cast<std::size_t>(axis)) >> 4U);
            const auto maxSteps = static_cast<float>(a_code.at(static_cast<std::size_t>(axis)) & 0x0FU);
            child.min[axis] += minSteps * minSteps * step[axis];
            child.max[axis] -= maxSteps * maxSteps * step[axis];
        }
        return child;
    }

    class MeshReader {
    public:
        MeshReader(const Packfile& a_packfile, const std::uint32_t a_shape)
            : _packfile(a_packfile)
            , _convexRadius(a_packfile.Read<float>(a_shape + kShapeConvexRadius)) {
            const auto data = a_packfile.Pointer(a_shape + kShapeData);
            _domain = {.min = a_packfile.Vector(data + kDomainMin), .max = a_packfile.Vector(data + kDomainMax)};
            _sections = a_packfile.Array(data + kSections);
            _primitives = a_packfile.Array(data + kPrimitives);
            _sharedVerticesIndex = a_packfile.Array(data + kSharedVerticesIndex);
            _packedVertices = a_packfile.Array(data + kPackedVertices);
            _sharedVertices = a_packfile.Array(data + kSharedVertices);
        }

        void Read(const glm::mat4& a_transform, ShapeGeometry& a_geometry) const {
            for (std::uint32_t i = 0; i < _sections.count; i++) {
                const auto section = ReadSection(Element(_sections, i, kSectionSize));
                const auto primitives = Read<std::uint32_t>(section.offset + kSectionPrimitives);
                // Primitive ranges pack a start in the high 24 bits and a count in the low 8.
                for (std::uint32_t primitive = 0; primitive < (primitives & 0xFFU); primitive++) {
                    ReadPrimitive(
                        section,
                        Element(_primitives, std::int64_t {primitives >> 8U} + primitive, kPrimitiveSize),
                        a_transform,
                        a_geometry
                    );
                }
            }
        }

    private:
        template <class T>
        [[nodiscard]] T Read(const std::uint64_t a_offset) const {
            return _packfile.Read<T>(a_offset);
        }

        [[nodiscard]] Section ReadSection(const std::uint32_t a_offset) const {
            Section section {.offset = a_offset};
            for (std::uint32_t i = 0; i < section.codec.size(); i++) {
                section.codec.at(i) = Read<float>(std::uint64_t {a_offset} + kSectionCodec + (std::uint64_t {i} * 4));
            }
            section.firstPackedVertex = Read<std::uint32_t>(std::uint64_t {a_offset} + kSectionFirstPackedVertex);
            section.packedVertexCount = Read<std::uint8_t>(std::uint64_t {a_offset} + kSectionPackedVertexCount);
            // The shared range starts in the high 24 bits and the low 8 repeat the packed vertex count.
            const auto shared = Read<std::uint32_t>(std::uint64_t {a_offset} + kSectionSharedVertices);
            section.firstSlot = std::int64_t {shared >> 8U} - (shared & 0xFFU);
            // Shared vertex indices are 16-bit, so each page addresses the next 65536 shared vertices.
            section.firstSharedVertex = std::uint32_t {Read<std::uint8_t>(std::uint64_t {a_offset} + kSectionPage)}
                                        << 16U;
            section.nodes = _packfile.Array(a_offset + kSectionNodes);
            section.domain = {
                .min = _packfile.Vector(a_offset + kSectionDomainMin),
                .max = _packfile.Vector(a_offset + kSectionDomainMax),
            };
            return section;
        }

        void ReadPrimitive(
            const Section& a_section,
            const std::uint32_t a_primitive,
            const glm::mat4& a_transform,
            ShapeGeometry& a_geometry
        ) const {
            std::array<std::uint8_t, 4> indices {};
            for (std::uint32_t corner = 0; corner < indices.size(); corner++) {
                indices.at(corner) = Read<std::uint8_t>(std::uint64_t {a_primitive} + corner);
            }

            if (indices == kDeadPrimitive) {
                return;
            }
            // A custom primitive repeats its second index, a tree node, in the last two slots.
            if (indices[1] == indices[2] && indices[2] == indices[3]) {
                ReadCustomPrimitive(a_section, indices[0], indices[1], a_transform, a_geometry);
                return;
            }

            // A triangle repeats its third index in the fourth slot.
            const auto cornerCount = indices[2] == indices[3] ? 3U : 4U;
            std::vector<glm::vec3> polygon;
            polygon.reserve(cornerCount);
            for (std::uint32_t corner = 0; corner < cornerCount; corner++) {
                polygon.push_back(Apply(a_transform, Vertex(a_section, indices.at(corner))));
            }
            a_geometry.polygons.push_back(std::move(polygon));
        }

        [[nodiscard]] std::uint16_t Slot(const Section& a_section, const std::uint32_t a_index) const {
            return Read<std::uint16_t>(Element(_sharedVerticesIndex, a_section.firstSlot + a_index, kSlotSize));
        }

        // Section vertices quantize x and y to 11 bits and z to 10 within the section's codec box.
        // Shared vertices quantize to 21, 21 and 22 bits within the mesh's domain.
        [[nodiscard]] glm::vec3 Vertex(const Section& a_section, const std::uint32_t a_index) const {
            if (a_index < a_section.packedVertexCount) {
                const auto packed = Read<std::uint32_t>(
                    Element(_packedVertices, std::int64_t {a_section.firstPackedVertex} + a_index, kPackedVertexSize)
                );
                return {
                    a_section.codec[0] + (static_cast<float>(packed & 0x7FFU) * a_section.codec[3]),
                    a_section.codec[1] + (static_cast<float>((packed >> 11U) & 0x7FFU) * a_section.codec[4]),
                    a_section.codec[2] + (static_cast<float>(packed >> 22U) * a_section.codec[5]),
                };
            }

            const auto shared = Read<std::uint64_t>(Element(
                _sharedVertices,
                std::int64_t {a_section.firstSharedVertex} + Slot(a_section, a_index),
                kSharedVertexSize
            ));
            return _domain.Point({
                static_cast<float>(shared & 0x1FFFFFU) / static_cast<float>(0x1FFFFFU),
                static_cast<float>((shared >> 21U) & 0x1FFFFFU) / static_cast<float>(0x1FFFFFU),
                static_cast<float>(shared >> 42U) / static_cast<float>(0x3FFFFFU),
            });
        }

        // Custom primitives are spheres, capsules and convex hulls. Their header and data sit in the index slots from
        // the primitive's first index: the header, the first vertex in the shared vertex stream, then the radius.
        void ReadCustomPrimitive(
            const Section& a_section,
            const std::uint32_t a_slot,
            const std::uint32_t a_node,
            const glm::mat4& a_transform,
            ShapeGeometry& a_geometry
        ) const {
            const std::uint32_t header = Slot(a_section, a_slot);
            const std::uint32_t method = (header >> 4U) & 0x03U;
            const std::uint32_t vertexCount = header >> 8U;
            const auto firstVertex = std::int64_t {a_section.firstSharedVertex} + Slot(a_section, a_slot + 1);
            // An explicit radius is a float's upper 16 bits.
            const auto radius = (header & kCustomRadiusMask) != 0
                                    ? std::bit_cast<float>(std::uint32_t {Slot(a_section, a_slot + 2)} << 16U)
                                    : _convexRadius;

            std::vector<glm::vec3> points;
            points.reserve(vertexCount);
            const auto box = method == 0 ? Box {} : NodeBox(a_section, a_node);
            for (std::uint32_t i = 0; i < vertexCount; i++) {
                points.push_back(Apply(a_transform, CustomVertex(method, firstVertex, i, box)));
            }

            const auto scale = RadiusScale(a_transform);
            switch (header & kCustomKindMask) {
                case kCustomSphere:
                    RequireVertices(points, 1);
                    a_geometry.spheres.push_back({.center = points[0], .radius = radius * scale});
                    break;
                case kCustomCapsule:
                    RequireVertices(points, 2);
                    a_geometry.capsules.push_back({.start = points[0], .end = points[1], .radius = radius * scale});
                    break;
                case kCustomConvex: a_geometry.hulls.push_back(std::move(points)); break;
                default:
                    throw std::runtime_error(
                        QStringLiteral("Havok compressed mesh custom primitive kind %1 is not supported")
                            .arg(header & kCustomKindMask)
                            .toStdString()
                    );
            }
        }

        static void RequireVertices(const std::vector<glm::vec3>& a_points, const std::size_t a_count) {
            if (a_points.size() < a_count) {
                throw std::runtime_error(
                    QStringLiteral("Havok compressed mesh custom primitive has %1 vertices, needs %2")
                        .arg(a_points.size())
                        .arg(a_count)
                        .toStdString()
                );
            }
        }

        // Method 0 stores full shared vertices. Methods 1 and 2 quantize to 11, 11 and 10 bits or to 5, 5 and 6 bits
        // within the primitive's tree node box, packed into the shared vertex stream in reversed word order.
        [[nodiscard]] glm::vec3 CustomVertex(
            const std::uint32_t a_method,
            const std::int64_t a_firstVertex,
            const std::uint32_t a_index,
            const Box& a_box
        ) const {
            switch (a_method) {
                case 0: {
                    const auto shared = Read<std::uint64_t>(
                        Element(_sharedVertices, a_firstVertex + a_index, kSharedVertexSize)
                    );
                    return _domain.Point({
                        static_cast<float>(shared & 0x1FFFFFU) / static_cast<float>(0x1FFFFFU),
                        static_cast<float>((shared >> 21U) & 0x1FFFFFU) / static_cast<float>(0x1FFFFFU),
                        static_cast<float>(shared >> 42U) / static_cast<float>(0x3FFFFFU),
                    });
                }
                case 1: {
                    const auto word = a_index ^ 1U;
                    const auto packed = Read<std::uint32_t>(
                        std::uint64_t {Element(_sharedVertices, a_firstVertex + (word / 2), kSharedVertexSize)}
                        + (std::uint64_t {word % 2} * 4)
                    );
                    return a_box.Point({
                        static_cast<float>(packed & 0x7FFU) / 2047.0F,
                        static_cast<float>((packed >> 11U) & 0x7FFU) / 2047.0F,
                        static_cast<float>(packed >> 22U) / 1023.0F,
                    });
                }
                case 2: {
                    const auto word = (a_index & ~3U) + 3U - (a_index & 3U);
                    const std::uint32_t packed = Read<std::uint16_t>(
                        std::uint64_t {Element(_sharedVertices, a_firstVertex + (word / 4), kSharedVertexSize)}
                        + (std::uint64_t {word % 4} * 2)
                    );
                    return a_box.Point({
                        static_cast<float>(packed & 0x1FU) / 31.0F,
                        static_cast<float>((packed >> 5U) & 0x1FU) / 31.0F,
                        static_cast<float>(packed >> 10U) / 63.0F,
                    });
                }
                default:
                    throw std::runtime_error(
                        QStringLiteral("Havok compressed mesh vertex compression method %1 is not supported")
                            .arg(a_method)
                            .toStdString()
                    );
            }
        }

        // Each node's left child follows it and its right child lies at the offset in its fourth byte. The walk to a
        // node narrows the section's box at every step.
        [[nodiscard]] Box NodeBox(const Section& a_section, const std::uint32_t a_target) const {
            auto box = a_section.domain;
            std::uint32_t node = 0;
            while (node != a_target) {
                const auto right = node + (Read<std::uint8_t>(Element(a_section.nodes, node, kNodeSize) + 3U) & 0xFEU);
                const auto next = right <= a_target ? right : node + 1;
                if (next <= node) {
                    throw std::runtime_error(
                        QStringLiteral("Havok compressed mesh tree node %1 has no child toward node %2")
                            .arg(node)
                            .arg(a_target)
                            .toStdString()
                    );
                }

                const auto child = Element(a_section.nodes, next, kNodeSize);
                box = ChildBox(
                    box,
                    {Read<std::uint8_t>(child), Read<std::uint8_t>(child + 1U), Read<std::uint8_t>(child + 2U)}
                );
                node = next;
            }
            return box;
        }

        const Packfile& _packfile;
        float _convexRadius = 0.0F;
        Box _domain;
        Packfile::Range _sections;
        Packfile::Range _primitives;
        Packfile::Range _sharedVerticesIndex;
        Packfile::Range _packedVertices;
        Packfile::Range _sharedVertices;
    };
} // namespace

void ReadCompressedMesh(
    const Packfile& a_packfile,
    const std::uint32_t a_shape,
    const glm::mat4& a_transform,
    ShapeGeometry& a_geometry
) {
    MeshReader(a_packfile, a_shape).Read(a_transform, a_geometry);
}
} // namespace Havok
