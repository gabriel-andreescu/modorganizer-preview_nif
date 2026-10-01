#include "Nif/CollisionGeometry.h"

#include "Nif/Havok/PhysicsSystem.h"

#include <nifly/ExtraData.hpp>
#include <nifly/Nodes.hpp>
#include <nifly/bhk.hpp>

#include <QDebug>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kEpsilon = 0.00001F;
constexpr int kCircleSegments = 32;
constexpr float kBodyScale = 7.0F;
constexpr float kSkyrimBhkScale = 10.0F;
// One game unit is 0.0142875 Havok units in Fallout 4.
constexpr float kFallout4HavokScale = 1.0F / 0.0142875F;

constexpr std::array<CollisionColor, 8> kLayerColors {
    {
        {.r = 0.0F, .g = 1.0F, .b = 0.0F, .a = 1.0F},
        {.r = 1.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F},
        {.r = 1.0F, .g = 0.0F, .b = 1.0F, .a = 1.0F},
        {.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = 1.0F},
        {.r = 0.5F, .g = 0.5F, .b = 1.0F, .a = 1.0F},
        {.r = 1.0F, .g = 0.8F, .b = 0.0F, .a = 1.0F},
        {.r = 1.0F, .g = 0.8F, .b = 0.4F, .a = 1.0F},
        {.r = 0.0F, .g = 1.0F, .b = 1.0F, .a = 1.0F},
    },
};
constexpr CollisionColor kBoundColor {.r = 1.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F};
constexpr CollisionColor kMultiBoundColor {.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = 0.8F};

nifly::Vector3 ToVector3(const nifly::Vector4& a_vector) {
    return {a_vector.x, a_vector.y, a_vector.z};
}

nifly::Vector3 Normalized(nifly::Vector3 a_vector) {
    if (a_vector.length() > kEpsilon) {
        a_vector.Normalize();
    }
    return a_vector;
}

nifly::Matrix3 MatrixFromQuaternion(const nifly::QuaternionXYZW& a_quat) {
    // glm matrices are column-major and nifly's are row-major.
    const auto rotation = glm::mat3_cast(glm::quat(a_quat.w, a_quat.x, a_quat.y, a_quat.z));
    return {
        rotation[0][0],
        rotation[1][0],
        rotation[2][0],
        rotation[0][1],
        rotation[1][1],
        rotation[2][1],
        rotation[0][2],
        rotation[1][2],
        rotation[2][2],
    };
}

nifly::MatTransform ScaleTransform(const float a_scale) {
    nifly::MatTransform transform;
    transform.Clear();
    transform.scale = a_scale;
    return transform;
}

// A Havok transform is stored column by column: the three rotation axes, then the translation.
nifly::MatTransform TransformFromMatrix(const nifly::Matrix4& a_matrix, const float a_translationScale = 1.0F) {
    nifly::MatTransform transform;
    transform.translation = nifly::Vector3(a_matrix[12], a_matrix[13], a_matrix[14]) * a_translationScale;

    const auto column0 = nifly::Vector3(a_matrix[0], a_matrix[1], a_matrix[2]);
    const auto column1 = nifly::Vector3(a_matrix[4], a_matrix[5], a_matrix[6]);
    const auto column2 = nifly::Vector3(a_matrix[8], a_matrix[9], a_matrix[10]);

    const auto scale = (column0.length() + column1.length() + column2.length()) / 3.0F;
    transform.scale = scale > kEpsilon ? scale : 1.0F;
    const auto axisX = column0 / transform.scale;
    const auto axisY = column1 / transform.scale;
    const auto axisZ = column2 / transform.scale;
    transform
        .rotation = nifly::Matrix3(axisX.x, axisY.x, axisZ.x, axisX.y, axisY.y, axisZ.y, axisX.z, axisY.z, axisZ.z);
    return transform;
}

bool UsesSkyrimHavokScale(const nifly::NifFile* a_nifFile) {
    if (!a_nifFile) {
        return false;
    }

    const auto& version = a_nifFile->GetHeader().GetVersion();
    return version.File() == nifly::NiFileVersion::V20_2_0_7 && version.User() >= 12;
}

CollisionColor BodyLayerColor(const nifly::bhkWorldObject* a_body) {
    if (!a_body) {
        return kLayerColors[0];
    }

    return kLayerColors[a_body->collisionFilter.layer & 7U];
}

nifly::Vector3 ToVector3(const glm::vec3& a_vector) {
    return {a_vector.x, a_vector.y, a_vector.z};
}

bool SameColor(const CollisionColor& a_lhs, const CollisionColor& a_rhs) {
    return std::fabs(a_lhs.r - a_rhs.r) <= kEpsilon
           && std::fabs(a_lhs.g - a_rhs.g) <= kEpsilon
           && std::fabs(a_lhs.b - a_rhs.b) <= kEpsilon
           && std::fabs(a_lhs.a - a_rhs.a) <= kEpsilon;
}

class Builder {
public:
    explicit Builder(const nifly::NifFile* a_nifFile)
        : _nifFile(a_nifFile)
        , _header(a_nifFile->GetHeader())
        , _havokScale(UsesSkyrimHavokScale(a_nifFile) ? kSkyrimBhkScale : 1.0F) {}

    CollisionGeometry Build() {
        if (auto* root = _nifFile->GetRootNode()) {
            nifly::MatTransform identity;
            identity.Clear();
            VisitObject(root, identity);
        }

        if (!_bounds.empty()) {
            _geometry.bounds = nifly::BoundingSphere(_bounds);
        }

        return std::move(_geometry);
    }

private:
    class ConvexHullEdgeEmitter {
    public:
        ConvexHullEdgeEmitter(
            Builder& a_builder,
            const std::vector<nifly::Vector3>& a_vertices,
            const nifly::MatTransform& a_transform,
            const CollisionColor& a_color
        )
            : _builder(a_builder)
            , _vertices(a_vertices)
            , _transform(a_transform)
            , _color(a_color) {}

        // Draws the outline of each hull face. Hull triangles that share a plane form one face, so an edge is drawn
        // only where its triangles' planes differ, which leaves out the diagonals inside flat faces.
        void AddEdges() {
            _tolerance = Extent() * kHullTolerance;
            for (std::size_t i = 0; i + 2 < _vertices.size(); i++) {
                for (std::size_t j = i + 1; j + 1 < _vertices.size(); j++) {
                    for (std::size_t k = j + 1; k < _vertices.size(); k++) {
                        AddTriangleIfHullFace(i, j, k);
                    }
                }
            }

            for (const auto& [edgeKey, normals] : _edgeNormals) {
                const auto bends = std::ranges::any_of(normals, [&](const nifly::Vector3& a_normal) {
                    return a_normal.dot(normals.front()) < 1.0F - kHullTolerance;
                });
                if (bends) {
                    AddEdgeLine(edgeKey);
                }
            }
            for (const auto edgeKey : _flatEdges) {
                AddEdgeLine(edgeKey);
            }
        }

    private:
        // Hull tests run relative to the hull's size, since Havok units, game units and float noise vary by shape.
        static constexpr float kHullTolerance = 0.0001F;

        [[nodiscard]] float Extent() const {
            nifly::Vector3 min = _vertices.front();
            nifly::Vector3 max = _vertices.front();
            for (const auto& vertex : _vertices) {
                min = nifly::Vector3(std::min(min.x, vertex.x), std::min(min.y, vertex.y), std::min(min.z, vertex.z));
                max = nifly::Vector3(std::max(max.x, vertex.x), std::max(max.y, vertex.y), std::max(max.z, vertex.z));
            }
            return std::max((max - min).length(), kEpsilon);
        }

        void AddTriangleIfHullFace(const std::size_t a_first, const std::size_t a_second, const std::size_t a_third) {
            const auto& first = _vertices[a_first];
            auto normal = (_vertices[a_second] - first).cross(_vertices[a_third] - first);
            if (normal.length() <= _tolerance * _tolerance) {
                return;
            }
            normal.Normalize();

            const auto side = HullSide(first, normal);
            if (!side) {
                return;
            }
            // Outward normals make the two faces at an edge compare equal only when they share a plane.
            if (*side != 0) {
                AddTriangle(a_first, a_second, a_third, normal * static_cast<float>(-*side));
                return;
            }

            // A flat hull has no outside. Its outline is the edges with every vertex on one side within the plane.
            AddFlatEdgeIfOutline(a_first, a_second, normal);
            AddFlatEdgeIfOutline(a_second, a_third, normal);
            AddFlatEdgeIfOutline(a_third, a_first, normal);
        }

        void AddFlatEdgeIfOutline(
            const std::size_t a_edgeStart,
            const std::size_t a_edgeEnd,
            const nifly::Vector3& a_normal
        ) {
            auto across = a_normal.cross(_vertices[a_edgeEnd] - _vertices[a_edgeStart]);
            across.Normalize();
            if (HullSide(_vertices[a_edgeStart], across)) {
                _flatEdges.insert(EdgeKey(a_edgeStart, a_edgeEnd));
            }
        }

        // The side of the plane every other vertex lies on: 1 or -1, 0 when all are on it, none when they straddle it.
        [[nodiscard]] std::optional<int> HullSide(const nifly::Vector3& a_point, const nifly::Vector3& a_normal) const {
            int side = 0;
            for (const auto& vertex : _vertices) {
                const auto distance = (vertex - a_point).dot(a_normal);
                if (std::fabs(distance) <= _tolerance) {
                    continue;
                }

                const auto vertexSide = distance > 0.0F ? 1 : -1;
                if (side != 0 && side != vertexSide) {
                    return std::nullopt;
                }
                side = vertexSide;
            }
            return side;
        }

        void AddTriangle(
            const std::size_t a_first,
            const std::size_t a_second,
            const std::size_t a_third,
            const nifly::Vector3& a_normal
        ) {
            AddEdgeNormal(a_first, a_second, a_normal);
            AddEdgeNormal(a_second, a_third, a_normal);
            AddEdgeNormal(a_third, a_first, a_normal);
        }

        void AddEdgeNormal(const std::size_t a_edgeStart, const std::size_t a_edgeEnd, const nifly::Vector3& a_normal) {
            _edgeNormals[EdgeKey(a_edgeStart, a_edgeEnd)].push_back(a_normal);
        }

        [[nodiscard]] static std::uint64_t EdgeKey(const std::size_t a_edgeStart, const std::size_t a_edgeEnd) {
            const auto minIndex = static_cast<std::uint64_t>(std::min(a_edgeStart, a_edgeEnd));
            const auto maxIndex = static_cast<std::uint64_t>(std::max(a_edgeStart, a_edgeEnd));
            return (minIndex << 32U) | maxIndex;
        }

        void AddEdgeLine(const std::uint64_t a_edgeKey) {
            _builder.AddLine(_transform, _vertices[a_edgeKey >> 32U], _vertices[a_edgeKey & 0xFFFFFFFFU], _color);
        }

        Builder& _builder;
        const std::vector<nifly::Vector3>& _vertices;
        const nifly::MatTransform& _transform;
        const CollisionColor& _color;
        float _tolerance = 0.0F;
        std::map<std::uint64_t, std::vector<nifly::Vector3>> _edgeNormals;
        std::set<std::uint64_t> _flatEdges;
    };

    void VisitObject(nifly::NiAVObject* a_object, const nifly::MatTransform& a_parent) {
        if (!a_object) {
            return;
        }

        const auto blockId = _nifFile->GetBlockID(a_object);
        if (blockId != nifly::NIF_NPOS && !_visitedObjects.insert(blockId).second) {
            return;
        }

        const auto world = a_parent.ComposeTransforms(a_object->GetTransformToParent());
        AddLegacyCollision(a_object, world);
        AddBhkCollision(a_object, world);
        AddBounds(a_object, world);
        AddMultiBound(a_object, world);

        if (auto* node = dynamic_cast<nifly::NiNode*>(a_object)) {
            for (const auto childRef : node->childRefs) {
                VisitObject(_header.GetBlock<nifly::NiAVObject>(childRef.index), world);
            }
        }
    }

    void AddLegacyCollision(nifly::NiAVObject* a_object, const nifly::MatTransform& a_transform) {
        auto* collision = _header.GetBlock<nifly::NiCollisionObject>(a_object->collisionRef);
        const auto* collisionData = dynamic_cast<nifly::NiCollisionData*>(collision);
        if (!collisionData) {
            return;
        }

        AddBoundingVolume(collisionData->boundingVolume, a_transform, kBoundColor);
    }

    void AddBhkCollision(nifly::NiAVObject* a_object, const nifly::MatTransform& a_transform) {
        auto* collision = _header.GetBlock<nifly::NiCollisionObject>(a_object->collisionRef);
        if (const auto* physicsCollision = dynamic_cast<nifly::bhkNPCollisionObject*>(collision)) {
            AddPhysicsSystemBody(*physicsCollision, a_transform);
            return;
        }

        const auto* bhkCollision = dynamic_cast<nifly::bhkNiCollisionObject*>(collision);
        if (!bhkCollision) {
            return;
        }

        auto* bodyObject = _header.GetBlock<nifly::NiObject>(bhkCollision->bodyRef);
        auto* body = dynamic_cast<nifly::bhkWorldObject*>(bodyObject);
        if (!body) {
            return;
        }

        auto bodyTransform = ScaleTransform(kBodyScale);
        if (const auto* rigidBodyT = dynamic_cast<nifly::bhkRigidBodyT*>(body)) {
            bodyTransform.rotation = MatrixFromQuaternion(rigidBodyT->rotation);
            bodyTransform.translation = ToVector3(rigidBodyT->translation) * kBodyScale * _havokScale;
        }

        AddBhkShape(
            _header.GetBlock<nifly::bhkShape>(body->shapeRef),
            a_transform.ComposeTransforms(bodyTransform),
            BodyLayerColor(body)
        );
    }

    void AddPhysicsSystemBody(const nifly::bhkNPCollisionObject& a_collision, const nifly::MatTransform& a_transform) {
        const auto* system = PhysicsSystem(a_collision.bodyRef.index);
        if (!system) {
            return;
        }

        try {
            const auto body = system->ReadBody(a_collision.bodyID);
            AddHavokGeometry(
                body.geometry,
                a_transform.ComposeTransforms(ScaleTransform(kFallout4HavokScale)),
                kLayerColors[body.collisionFilterInfo & 7U]
            );
        } catch (const std::exception& error) {
            qWarning("Skipping Fallout 4 collision body %u: %s", a_collision.bodyID, error.what());
        }
    }

    // Every collision object of a model usually shares one physics system, so each is parsed once.
    const Havok::PhysicsSystem* PhysicsSystem(const std::uint32_t a_blockId) {
        auto [entry, inserted] = _physicsSystems.try_emplace(a_blockId);
        if (inserted) {
            if (const auto bytes = PhysicsSystemBytes(a_blockId)) {
                try {
                    entry->second.emplace(*bytes);
                } catch (const std::exception& error) {
                    qWarning("Skipping Fallout 4 physics system block %u: %s", a_blockId, error.what());
                }
            }
        }
        return entry->second ? &*entry->second : nullptr;
    }

    // Skeletons keep their bones' bodies in a ragdoll system, which stores the same data as a physics system.
    [[nodiscard]] std::optional<std::vector<char>> PhysicsSystemBytes(const std::uint32_t a_blockId) const {
        if (const auto* system = _header.GetBlock<nifly::bhkPhysicsSystem>(a_blockId)) {
            return std::vector<char>(system->data.cbegin(), system->data.cend());
        }
        if (const auto* ragdoll = _header.GetBlock<nifly::bhkRagdollSystem>(a_blockId)) {
            return std::vector<char>(ragdoll->data.cbegin(), ragdoll->data.cend());
        }
        return std::nullopt;
    }

    void AddHavokGeometry(
        const Havok::ShapeGeometry& a_geometry,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        for (const auto& polygon : a_geometry.polygons) {
            for (std::size_t i = 0; i < polygon.size(); i++) {
                AddLine(a_transform, ToVector3(polygon[i]), ToVector3(polygon[(i + 1) % polygon.size()]), a_color);
            }
        }
        for (const auto& hull : a_geometry.hulls) {
            std::vector<nifly::Vector3> vertices;
            vertices.reserve(hull.size());
            for (const auto& vertex : hull) {
                vertices.push_back(ToVector3(vertex));
            }
            AddConvexHullEdges(vertices, a_transform, a_color);
        }
        for (const auto& sphere : a_geometry.spheres) {
            AddSphere(a_transform, ToVector3(sphere.center), sphere.radius, a_color);
        }
        for (const auto& capsule : a_geometry.capsules) {
            AddCapsule(a_transform, ToVector3(capsule.start), ToVector3(capsule.end), capsule.radius, a_color);
        }
    }

    void AddBounds(nifly::NiAVObject* a_object, const nifly::MatTransform& a_transform) {
        for (const auto extraDataRef : a_object->extraDataRefs) {
            if (const auto* bound = _header.GetBlock<nifly::BSBound>(extraDataRef.index)) {
                AddBox(
                    a_transform,
                    bound->center - bound->halfExtents,
                    bound->center + bound->halfExtents,
                    kBoundColor
                );
            }
        }
    }

    void AddMultiBound(nifly::NiAVObject* a_object, const nifly::MatTransform& a_transform) {
        const auto* multiBoundNode = dynamic_cast<nifly::BSMultiBoundNode*>(a_object);
        if (!multiBoundNode) {
            return;
        }

        const auto* multiBound = _header.GetBlock<nifly::BSMultiBound>(multiBoundNode->multiBoundRef);
        if (!multiBound) {
            return;
        }

        auto* data = _header.GetBlock<nifly::BSMultiBoundData>(multiBound->dataRef);
        if (const auto* aabb = dynamic_cast<nifly::BSMultiBoundAABB*>(data)) {
            AddBox(a_transform, aabb->center - aabb->halfExtent, aabb->center + aabb->halfExtent, kMultiBoundColor);
        } else if (const auto* obb = dynamic_cast<nifly::BSMultiBoundOBB*>(data)) {
            nifly::MatTransform obbTransform;
            obbTransform.translation = obb->center;
            obbTransform.rotation = obb->rotation;
            obbTransform.scale = 1.0F;
            AddBox(a_transform.ComposeTransforms(obbTransform), obb->size * -1.0F, obb->size, kMultiBoundColor);
        } else if (const auto* sphere = dynamic_cast<nifly::BSMultiBoundSphere*>(data)) {
            AddSphere(a_transform, sphere->center, sphere->radius, kMultiBoundColor);
        }
    }

    void AddBhkShape(nifly::bhkShape* a_shape, const nifly::MatTransform& a_transform, const CollisionColor& a_color) {
        if (!a_shape) {
            return;
        }

        const auto blockId = _nifFile->GetBlockID(a_shape);
        if (blockId != nifly::NIF_NPOS && !_shapeStack.insert(blockId).second) {
            return;
        }

        if (auto* list = dynamic_cast<nifly::bhkListShape*>(a_shape)) {
            for (const auto subShapeRef : list->subShapeRefs) {
                AddBhkShape(_header.GetBlock<nifly::bhkShape>(subShapeRef.index), a_transform, a_color);
            }
        } else if (auto* convexList = dynamic_cast<nifly::bhkConvexListShape*>(a_shape)) {
            for (const auto shapeRef : convexList->shapeRefs) {
                AddBhkShape(_header.GetBlock<nifly::bhkShape>(shapeRef.index), a_transform, a_color);
            }
        } else if (const auto* transformed = dynamic_cast<nifly::bhkTransformShape*>(a_shape)) {
            const auto localTransform = TransformFromMatrix(transformed->xform, _havokScale);
            AddBhkShape(
                _header.GetBlock<nifly::bhkShape>(transformed->shapeRef),
                a_transform.ComposeTransforms(localTransform),
                a_color
            );
        } else if (const auto* mopp = dynamic_cast<nifly::bhkMoppBvTreeShape*>(a_shape)) {
            AddBhkShape(_header.GetBlock<nifly::bhkShape>(mopp->shapeRef), a_transform, a_color);
        } else if (const auto* sphereShape = dynamic_cast<nifly::bhkSphereShape*>(a_shape)) {
            AddSphere(a_transform, {}, sphereShape->radius * _havokScale, a_color);
            // nifly's NiVector has no const begin().
            // NOLINTNEXTLINE(misc-const-correctness)
        } else if (auto* multiSphere = dynamic_cast<nifly::bhkMultiSphereShape*>(a_shape)) {
            for (const auto& sphere : multiSphere->spheres) {
                AddSphere(a_transform, sphere.center * _havokScale, sphere.radius * _havokScale, a_color);
            }
        } else if (const auto* box = dynamic_cast<nifly::bhkBoxShape*>(a_shape)) {
            const auto dimensions = box->dimensions * _havokScale;
            AddBox(a_transform, dimensions * -1.0F, dimensions, a_color);
        } else if (const auto* capsule = dynamic_cast<nifly::bhkCapsuleShape*>(a_shape)) {
            AddCapsule(
                a_transform,
                capsule->point1 * _havokScale,
                capsule->point2 * _havokScale,
                std::max(capsule->radius1, capsule->radius2) * _havokScale,
                a_color
            );
        } else if (auto* strips = dynamic_cast<nifly::bhkNiTriStripsShape*>(a_shape)) {
            AddNiTriStripsShape(strips, a_transform.ComposeTransforms(ScaleTransform(1.0F / kBodyScale)), a_color);
        } else if (auto* convex = dynamic_cast<nifly::bhkConvexVerticesShape*>(a_shape)) {
            AddConvexVerticesShape(convex, a_transform, a_color);
        } else if (auto* packed = dynamic_cast<nifly::bhkPackedNiTriStripsShape*>(a_shape)) {
            AddPackedNiTriStripsShape(packed, a_transform, a_color);
        } else if (auto* packedData = dynamic_cast<nifly::hkPackedNiTriStripsData*>(a_shape)) {
            AddPackedNiTriStripsData(packedData, a_transform, a_color);
        } else if (auto* compressed = dynamic_cast<nifly::bhkCompressedMeshShape*>(a_shape)) {
            AddCompressedMeshShape(compressed, a_transform, a_color);
        }

        if (blockId != nifly::NIF_NPOS) {
            _shapeStack.erase(blockId);
        }
    }

    void AddNiTriStripsShape(
        nifly::bhkNiTriStripsShape* a_shape,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        for (const auto partRef : a_shape->partRefs) {
            const auto* data = _header.GetBlock<nifly::NiTriStripsData>(partRef.index);
            if (!data) {
                continue;
            }

            std::vector<nifly::Triangle> triangles;
            data->GetTriangles(triangles);
            AddTriangleEdges(data->vertices, triangles, a_transform, a_color);
        }
    }

    void AddPackedNiTriStripsShape(
        nifly::bhkPackedNiTriStripsShape* a_shape,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        AddPackedNiTriStripsData(
            _header.GetBlock<nifly::hkPackedNiTriStripsData>(a_shape->dataRef),
            a_transform,
            a_color
        );
    }

    void AddPackedNiTriStripsData(
        nifly::hkPackedNiTriStripsData* a_data,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        if (!a_data) {
            return;
        }

        for (const auto& triangle : a_data->triData) {
            AddTriangleEdges(a_data->compressedVertData, triangle.tri, a_transform, a_color);
        }
        for (const auto& triangle : a_data->triNormData) {
            AddTriangleEdges(a_data->compressedVertData, triangle.tri, a_transform, a_color);
        }
    }

    void AddConvexVerticesShape(
        nifly::bhkConvexVerticesShape* a_shape,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        std::vector<nifly::Vector3> vertices;
        vertices.reserve(a_shape->verts.size());
        for (const auto& vertex : a_shape->verts) {
            vertices.push_back(ToVector3(vertex) * _havokScale);
        }

        AddConvexHullEdges(vertices, a_transform, a_color);
    }

    void AddConvexHullEdges(
        const std::vector<nifly::Vector3>& a_vertices,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        ConvexHullEdgeEmitter emitter(*this, a_vertices, a_transform, a_color);
        emitter.AddEdges();
    }

    void AddCompressedMeshShape(
        nifly::bhkCompressedMeshShape* a_shape,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        auto* data = _header.GetBlock<nifly::bhkCompressedMeshShapeData>(a_shape->dataRef);
        if (!data) {
            return;
        }

        for (const auto& triangle : data->bigTris) {
            if (triangle.triangle1 < data->bigVerts.size()
                && triangle.triangle2 < data->bigVerts.size()
                && triangle.triangle3 < data->bigVerts.size()) {
                AddTriangleEdges(
                    a_transform,
                    ToVector3(data->bigVerts[triangle.triangle1]) * _havokScale,
                    ToVector3(data->bigVerts[triangle.triangle2]) * _havokScale,
                    ToVector3(data->bigVerts[triangle.triangle3]) * _havokScale,
                    a_color
                );
            }
        }

        for (auto& chunk : data->chunks) {
            AddCompressedMeshChunk(data, chunk, a_transform, a_color);
        }
    }

    void AddCompressedMeshChunk(
        nifly::bhkCompressedMeshShapeData* a_data,
        nifly::bhkCMSDChunk& a_chunk,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        nifly::Vector3 transformTranslation;
        nifly::Matrix3 transformRotation;
        if (a_chunk.transformIndex < a_data->transforms.size()) {
            const auto& chunkTransform = a_data->transforms[a_chunk.transformIndex];
            transformTranslation = ToVector3(chunkTransform.translation);
            transformRotation = MatrixFromQuaternion(chunkTransform.rotation);
        }

        std::vector<nifly::Vector3> vertices(a_chunk.verts.size() / 3);
        for (std::size_t i = 0; i < vertices.size(); i++) {
            const auto offset = nifly::Vector3(
                                    static_cast<float>(a_chunk.verts[static_cast<std::uint32_t>(3 * i)]),
                                    static_cast<float>(a_chunk.verts[static_cast<std::uint32_t>((3 * i) + 1)]),
                                    static_cast<float>(a_chunk.verts[static_cast<std::uint32_t>((3 * i) + 2)])
                                )
                                / 1000.0F;
            vertices[i] = transformRotation
                          * ((ToVector3(a_chunk.translation) + transformTranslation + offset) * _havokScale);
        }

        std::size_t offset = 0;
        for (const auto stripLength : a_chunk.strips) {
            if (stripLength >= 3 && offset + stripLength <= a_chunk.indices.size()) {
                for (std::size_t i = 0; i + 2 < stripLength; i++) {
                    AddTriangleEdges(
                        vertices,
                        a_chunk.indices[static_cast<std::uint32_t>(offset + i)],
                        a_chunk.indices[static_cast<std::uint32_t>(offset + i + 1)],
                        a_chunk.indices[static_cast<std::uint32_t>(offset + i + 2)],
                        a_transform,
                        a_color
                    );
                }
            }
            offset += stripLength;
        }

        for (std::size_t i = offset; i + 2 < a_chunk.indices.size(); i += 3) {
            AddTriangleEdges(
                vertices,
                a_chunk.indices[static_cast<std::uint32_t>(i)],
                a_chunk.indices[static_cast<std::uint32_t>(i + 1)],
                a_chunk.indices[static_cast<std::uint32_t>(i + 2)],
                a_transform,
                a_color
            );
        }
    }

    void AddBoundingVolume(
        const nifly::BoundingVolume& a_volume,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        switch (a_volume.collisionType) {
            case nifly::SPHERE_BV:
                AddSphere(a_transform, a_volume.bvSphere.center, a_volume.bvSphere.radius, a_color);
                break;
            case nifly::BOX_BV:     AddOrientedBox(a_transform, a_volume.bvBox, a_color); break;
            case nifly::CAPSULE_BV: {
                const auto capsuleAxis = Normalized(a_volume.bvCapsule.origin) * a_volume.bvCapsule.extent;
                AddCapsule(
                    a_transform,
                    a_volume.bvCapsule.center - capsuleAxis,
                    a_volume.bvCapsule.center + capsuleAxis,
                    a_volume.bvCapsule.radius,
                    a_color
                );
                break;
            }
            case nifly::UNION_BV:
                if (a_volume.bvUnion) {
                    for (const auto& child : a_volume.bvUnion->boundingVolumes) {
                        AddBoundingVolume(child, a_transform, a_color);
                    }
                }
                break;
            case nifly::HALFSPACE_BV:
                qWarning("Skipping unsupported NiCollisionData half-space bounding volume");
                break;
            default: break;
        }
    }

    void AddBox(
        const nifly::MatTransform& a_transform,
        const nifly::Vector3& a_min,
        const nifly::Vector3& a_max,
        const CollisionColor& a_color
    ) {
        AddBoxEdges(
            a_transform,
            {
                {
                    {a_min.x, a_min.y, a_min.z},
                    {a_max.x, a_min.y, a_min.z},
                    {a_max.x, a_max.y, a_min.z},
                    {a_min.x, a_max.y, a_min.z},
                    {a_min.x, a_min.y, a_max.z},
                    {a_max.x, a_min.y, a_max.z},
                    {a_max.x, a_max.y, a_max.z},
                    {a_min.x, a_max.y, a_max.z},
                },
            },
            a_color
        );
    }

    void AddOrientedBox(
        const nifly::MatTransform& a_transform,
        const nifly::BoxBV& a_box,
        const CollisionColor& a_color
    ) {
        const auto half1 = a_box.axis1 * a_box.extent1;
        const auto half2 = a_box.axis2 * a_box.extent2;
        const auto half3 = a_box.axis3 * a_box.extent3;
        const auto& center = a_box.center;

        AddBoxEdges(
            a_transform,
            {
                {
                    center - half1 - half2 - half3,
                    center + half1 - half2 - half3,
                    center + half1 + half2 - half3,
                    center - half1 + half2 - half3,
                    center - half1 - half2 + half3,
                    center + half1 - half2 + half3,
                    center + half1 + half2 + half3,
                    center - half1 + half2 + half3,
                },
            },
            a_color
        );
    }

    void AddBoxEdges(
        const nifly::MatTransform& a_transform,
        const std::array<nifly::Vector3, 8>& a_points,
        const CollisionColor& a_color
    ) {
        static constexpr std::array<std::array<int, 2>, 12> kEdges {
            {
                {0, 1},
                {1, 2},
                {2, 3},
                {3, 0},
                {4, 5},
                {5, 6},
                {6, 7},
                {7, 4},
                {0, 4},
                {1, 5},
                {2, 6},
                {3, 7},
            },
        };

        for (const auto& edge : kEdges) {
            AddLine(a_transform, a_points[edge[0]], a_points[edge[1]], a_color);
        }
    }

    void AddSphere(
        const nifly::MatTransform& a_transform,
        const nifly::Vector3& a_center,
        const float a_radius,
        const CollisionColor& a_color
    ) {
        if (a_radius <= kEpsilon) {
            return;
        }

        AddCircle(a_transform, a_center, {a_radius, 0.0F, 0.0F}, {0.0F, a_radius, 0.0F}, a_color);
        AddCircle(a_transform, a_center, {a_radius, 0.0F, 0.0F}, {0.0F, 0.0F, a_radius}, a_color);
        AddCircle(a_transform, a_center, {0.0F, a_radius, 0.0F}, {0.0F, 0.0F, a_radius}, a_color);
    }

    void AddCapsule(
        const nifly::MatTransform& a_transform,
        const nifly::Vector3& a_point1,
        const nifly::Vector3& a_point2,
        const float a_radius,
        const CollisionColor& a_color
    ) {
        if (a_radius <= kEpsilon) {
            AddLine(a_transform, a_point1, a_point2, a_color);
            return;
        }

        const auto axis = a_point2 - a_point1;
        if (axis.length() <= kEpsilon) {
            AddSphere(a_transform, a_point1, a_radius, a_color);
            return;
        }

        const auto direction = Normalized(axis);
        const auto seed = std::fabs(direction.x) < 0.75F ? nifly::Vector3(1.0F, 0.0F, 0.0F)
                                                         : nifly::Vector3(0.0F, 1.0F, 0.0F);
        const auto sideA = Normalized(direction.cross(seed)) * a_radius;
        const auto sideB = Normalized(direction.cross(sideA)) * a_radius;

        AddCircle(a_transform, a_point1, sideA, sideB, a_color);
        AddCircle(a_transform, a_point2, sideA, sideB, a_color);
        AddLine(a_transform, a_point1 + sideA, a_point2 + sideA, a_color);
        AddLine(a_transform, a_point1 - sideA, a_point2 - sideA, a_color);
        AddLine(a_transform, a_point1 + sideB, a_point2 + sideB, a_color);
        AddLine(a_transform, a_point1 - sideB, a_point2 - sideB, a_color);
    }

    void AddCircle(
        const nifly::MatTransform& a_transform,
        const nifly::Vector3& a_center,
        const nifly::Vector3& a_axisA,
        const nifly::Vector3& a_axisB,
        const CollisionColor& a_color
    ) {
        for (int i = 0; i < kCircleSegments; i++) {
            const auto angleA = 2.0F * kPi * static_cast<float>(i) / static_cast<float>(kCircleSegments);
            const auto angleB = 2.0F * kPi * static_cast<float>(i + 1) / static_cast<float>(kCircleSegments);
            AddLine(
                a_transform,
                a_center + a_axisA * std::cos(angleA) + a_axisB * std::sin(angleA),
                a_center + a_axisA * std::cos(angleB) + a_axisB * std::sin(angleB),
                a_color
            );
        }
    }

    void AddTriangleEdges(
        const std::vector<nifly::Vector3>& a_vertices,
        const std::vector<nifly::Triangle>& a_triangles,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        for (const auto& triangle : a_triangles) {
            AddTriangleEdges(a_vertices, triangle, a_transform, a_color);
        }
    }

    void AddTriangleEdges(
        const std::vector<nifly::Vector3>& a_vertices,
        const nifly::Triangle& a_triangle,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        AddTriangleEdges(a_vertices, a_triangle.p1, a_triangle.p2, a_triangle.p3, a_transform, a_color);
    }

    void AddTriangleEdges(
        const std::vector<nifly::Vector3>& a_vertices,
        const std::uint16_t a_i1,
        const std::uint16_t a_i2,
        const std::uint16_t a_i3,
        const nifly::MatTransform& a_transform,
        const CollisionColor& a_color
    ) {
        if (a_i1 >= a_vertices.size()
            || a_i2 >= a_vertices.size()
            || a_i3 >= a_vertices.size()
            || (a_i1 == a_i2 && a_i2 == a_i3)) {
            return;
        }

        AddTriangleEdges(a_transform, a_vertices[a_i1], a_vertices[a_i2], a_vertices[a_i3], a_color);
    }

    void AddTriangleEdges(
        const nifly::MatTransform& a_transform,
        const nifly::Vector3& a_point1,
        const nifly::Vector3& a_point2,
        const nifly::Vector3& a_point3,
        const CollisionColor& a_color
    ) {
        AddLine(a_transform, a_point1, a_point2, a_color);
        AddLine(a_transform, a_point2, a_point3, a_color);
        AddLine(a_transform, a_point3, a_point1, a_color);
    }

    void AddLine(
        const nifly::MatTransform& a_transform,
        const nifly::Vector3& a_start,
        const nifly::Vector3& a_end,
        const CollisionColor& a_color
    ) {
        const auto firstVertex = _geometry.vertices.size();
        AddVertex(a_transform.ApplyTransform(a_start), a_color);
        AddVertex(a_transform.ApplyTransform(a_end), a_color);
        AddLineRange(firstVertex, 2, a_color);
    }

    void AddVertex(const nifly::Vector3& a_point, const CollisionColor& a_color) {
        _geometry.vertices.push_back({
            .x = a_point.x,
            .y = a_point.y,
            .z = a_point.z,
            .r = a_color.r,
            .g = a_color.g,
            .b = a_color.b,
            .a = a_color.a,
        });
        _bounds.push_back(a_point);
    }

    void AddLineRange(const std::size_t a_firstVertex, const std::size_t a_vertexCount, const CollisionColor& a_color) {
        if (!_geometry.lineRanges.empty()) {
            auto& range = _geometry.lineRanges.back();
            if (SameColor(range.color, a_color) && range.firstVertex + range.vertexCount == a_firstVertex) {
                range.vertexCount += a_vertexCount;
                return;
            }
        }

        _geometry.lineRanges.push_back({.firstVertex = a_firstVertex, .vertexCount = a_vertexCount, .color = a_color});
    }

    const nifly::NifFile* _nifFile = nullptr;
    const nifly::NiHeader& _header;
    float _havokScale = 1.0F;
    CollisionGeometry _geometry;
    std::vector<nifly::Vector3> _bounds;
    std::unordered_set<std::uint32_t> _visitedObjects;
    std::unordered_set<std::uint32_t> _shapeStack;
    std::unordered_map<std::uint32_t, std::optional<Havok::PhysicsSystem>> _physicsSystems;
};
} // namespace

CollisionGeometry CollisionGeometryBuilder::Build(const nifly::NifFile* a_nifFile) {
    if (!a_nifFile || !a_nifFile->IsValid()) {
        return {};
    }

    return Builder(a_nifFile).Build();
}
