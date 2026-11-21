#include "Nif/Havok/Shapes.h"

#include "Nif/Havok/CompressedMesh.h"
#include "Nif/Havok/Packfile.h"
#include "Nif/Havok/Transform.h"

#include <QString>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>

#include <stdexcept>
#include <string_view>

namespace Havok {
namespace {
    constexpr std::uint32_t kVectorSize = 16;

    constexpr std::uint32_t kConvexRadius = 0x14;
    constexpr std::uint32_t kConvexVertices = 0x30;
    constexpr std::uint32_t kPolytopeFaces = 0x44;
    constexpr std::uint32_t kPolytopeIndices = 0x48;
    constexpr std::uint32_t kFaceSize = 4;
    constexpr std::uint32_t kCapsuleStart = 0x50;
    constexpr std::uint32_t kCapsuleEnd = 0x60;
    constexpr std::uint32_t kScaledChild = 0x30;
    constexpr std::uint32_t kScaledScale = 0x40;
    constexpr std::uint32_t kScaledTranslation = 0x50;
    constexpr std::uint32_t kCompoundInstances = 0x60;
    constexpr std::uint32_t kInstanceSize = 0x80;
    constexpr std::uint32_t kInstanceScale = 0x40;
    constexpr std::uint32_t kInstanceShape = 0x50;

    // Vanilla shapes nest at most three deep, a compound holding scaled convex shapes. Deeper means a pointer cycle.
    constexpr int kMaxDepth = 16;

    QString ClassName(const std::string_view a_className) {
        return QString::fromUtf8(a_className.data(), static_cast<qsizetype>(a_className.size()));
    }

    class ShapeReader {
    public:
        ShapeReader(const Packfile& a_packfile, ShapeGeometry& a_geometry)
            : _packfile(a_packfile)
            , _geometry(a_geometry) {}

        void Read(const std::uint32_t a_shape, const glm::mat4& a_transform, const int a_depth) {
            if (a_depth > kMaxDepth) {
                throw std::runtime_error(QStringLiteral("Havok shapes nest deeper than %1 levels at data offset %2")
                        .arg(kMaxDepth)
                        .arg(a_shape)
                        .toStdString());
            }

            const auto className = _packfile.ClassOf(a_shape);
            if (className == "hknpConvexPolytopeShape") {
                ReadPolytope(a_shape, a_transform);
            } else if (className == "hknpConvexShape") {
                ReadHull(a_shape, a_transform);
            } else if (className == "hknpSphereShape") {
                ReadSphere(a_shape, a_transform);
            } else if (className == "hknpCapsuleShape") {
                ReadCapsule(a_shape, a_transform);
            } else if (className == "hknpScaledConvexShape") {
                ReadScaledConvex(a_shape, a_transform, a_depth);
            } else if (className == "hknpDynamicCompoundShape" || className == "hknpStaticCompoundShape") {
                ReadCompound(a_shape, a_transform, a_depth);
            } else if (className == "hknpCompressedMeshShape") {
                ReadCompressedMesh(_packfile, a_shape, a_transform, _geometry);
            } else {
                throw std::runtime_error(
                    QStringLiteral("Havok shape class %1 is not supported").arg(ClassName(className)).toStdString()
                );
            }
        }

    private:
        [[nodiscard]] glm::vec3 ConvexVertex(const Packfile::Range& a_vertices, const std::uint32_t a_index) const {
            if (a_index >= a_vertices.count) {
                throw std::runtime_error(QStringLiteral("Havok convex vertex %1 is outside its %2 vertices")
                        .arg(a_index)
                        .arg(a_vertices.count)
                        .toStdString());
            }
            return _packfile.Vector(a_vertices.offset + (a_index * kVectorSize));
        }

        void ReadPolytope(const std::uint32_t a_shape, const glm::mat4& a_transform) {
            const auto vertices = _packfile.RelArray(a_shape + kConvexVertices);
            const auto faces = _packfile.RelArray(a_shape + kPolytopeFaces);
            const auto indices = _packfile.RelArray(a_shape + kPolytopeIndices);
            for (std::uint32_t face = 0; face < faces.count; face++) {
                const auto offset = std::uint64_t {faces.offset} + (std::uint64_t {face} * kFaceSize);
                const auto firstIndex = _packfile.Read<std::uint16_t>(offset);
                const auto indexCount = _packfile.Read<std::uint8_t>(offset + 2);
                if (std::uint32_t {firstIndex} + indexCount > indices.count) {
                    throw std::runtime_error(QStringLiteral("Havok convex face %1 reads past its %2 indices")
                            .arg(face)
                            .arg(indices.count)
                            .toStdString());
                }

                std::vector<glm::vec3> polygon;
                polygon.reserve(indexCount);
                for (std::uint32_t i = 0; i < indexCount; i++) {
                    const auto vertex = _packfile.Read<std::uint8_t>(std::uint64_t {indices.offset} + firstIndex + i);
                    polygon.push_back(Apply(a_transform, ConvexVertex(vertices, vertex)));
                }
                _geometry.polygons.push_back(std::move(polygon));
            }
        }

        void ReadHull(const std::uint32_t a_shape, const glm::mat4& a_transform) {
            const auto vertices = _packfile.RelArray(a_shape + kConvexVertices);
            std::vector<glm::vec3> hull;
            hull.reserve(vertices.count);
            for (std::uint32_t i = 0; i < vertices.count; i++) {
                hull.push_back(Apply(a_transform, ConvexVertex(vertices, i)));
            }
            _geometry.hulls.push_back(std::move(hull));
        }

        // A sphere is its first vertex grown by the convex radius.
        void ReadSphere(const std::uint32_t a_shape, const glm::mat4& a_transform) {
            _geometry.spheres.push_back({
                .center = Apply(a_transform, ConvexVertex(_packfile.RelArray(a_shape + kConvexVertices), 0)),
                .radius = _packfile.Read<float>(a_shape + kConvexRadius) * RadiusScale(a_transform),
            });
        }

        void ReadCapsule(const std::uint32_t a_shape, const glm::mat4& a_transform) {
            _geometry.capsules.push_back({
                .start = Apply(a_transform, _packfile.Vector(a_shape + kCapsuleStart)),
                .end = Apply(a_transform, _packfile.Vector(a_shape + kCapsuleEnd)),
                .radius = _packfile.Read<float>(a_shape + kConvexRadius) * RadiusScale(a_transform),
            });
        }

        void ReadScaledConvex(const std::uint32_t a_shape, const glm::mat4& a_transform, const int a_depth) {
            const auto scaled = glm::scale(
                glm::translate(a_transform, _packfile.Vector(a_shape + kScaledTranslation)),
                _packfile.Vector(a_shape + kScaledScale)
            );
            Read(_packfile.Pointer(a_shape + kScaledChild), scaled, a_depth + 1);
        }

        // Instances store their rotation as three columns, then the translation. The w components hold flags.
        void ReadCompound(const std::uint32_t a_shape, const glm::mat4& a_transform, const int a_depth) {
            const auto instances = _packfile.Array(a_shape + kCompoundInstances);
            for (std::uint32_t i = 0; i < instances.count; i++) {
                const auto instance = instances.offset + (i * kInstanceSize);
                // Free slots of the instance list have no shape.
                if (!_packfile.HasPointer(instance + kInstanceShape)) {
                    continue;
                }

                glm::mat4 local(1.0F);
                for (std::uint32_t column = 0; column < 4; column++) {
                    local[static_cast<glm::length_t>(column)] = glm::vec4(
                        _packfile.Vector(instance + (column * kVectorSize)),
                        column == 3 ? 1.0F : 0.0F
                    );
                }
                local = glm::scale(local, _packfile.Vector(instance + kInstanceScale));
                Read(_packfile.Pointer(instance + kInstanceShape), a_transform * local, a_depth + 1);
            }
        }

        const Packfile& _packfile;
        ShapeGeometry& _geometry;
    };
} // namespace

void ReadShape(
    const Packfile& a_packfile,
    const std::uint32_t a_shape,
    const glm::mat4& a_transform,
    ShapeGeometry& a_geometry
) {
    ShapeReader(a_packfile, a_geometry).Read(a_shape, a_transform, 0);
}
} // namespace Havok
