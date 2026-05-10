#include "CollisionGeometry.h"

#include <ExtraData.hpp>
#include <Nodes.hpp>
#include <bhk.hpp>

#include <QDebug>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <unordered_set>
#include <vector>

namespace {
constexpr float Pi = std::numbers::pi_v<float>;
constexpr float Epsilon = 0.00001f;
constexpr int CircleSegments = 32;
constexpr float BodyScale = 7.0f;
constexpr float SkyrimBhkScale = 10.0f;

constexpr std::array<CollisionColor, 8> LayerColors {{
    {.r = 0.0f, .g = 1.0f, .b = 0.0f, .a = 1.0f},
    {.r = 1.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f},
    {.r = 1.0f, .g = 0.0f, .b = 1.0f, .a = 1.0f},
    {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f},
    {.r = 0.5f, .g = 0.5f, .b = 1.0f, .a = 1.0f},
    {.r = 1.0f, .g = 0.8f, .b = 0.0f, .a = 1.0f},
    {.r = 1.0f, .g = 0.8f, .b = 0.4f, .a = 1.0f},
    {.r = 0.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f},
}};
constexpr CollisionColor BoundColor {.r = 1.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};
constexpr CollisionColor MultiBoundColor {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 0.8f};

nifly::Vector3 toVector3(const nifly::Vector4& vector) {
    return {vector.x, vector.y, vector.z};
}

nifly::Vector3 normalized(nifly::Vector3 vector) {
    if (vector.length() > Epsilon) {
        vector.Normalize();
    }
    return vector;
}

nifly::Matrix3 matrixFromQuaternion(const nifly::QuaternionXYZW& quat) {
    const auto x = quat.x;
    const auto y = quat.y;
    const auto z = quat.z;
    const auto w = quat.w;
    const auto xx = x * x;
    const auto yy = y * y;
    const auto zz = z * z;
    const auto xy = x * y;
    const auto xz = x * z;
    const auto yz = y * z;
    const auto wx = w * x;
    const auto wy = w * y;
    const auto wz = w * z;

    return {
        1.0f - 2.0f * (yy + zz),
        2.0f * (xy - wz),
        2.0f * (xz + wy),
        2.0f * (xy + wz),
        1.0f - 2.0f * (xx + zz),
        2.0f * (yz - wx),
        2.0f * (xz - wy),
        2.0f * (yz + wx),
        1.0f - 2.0f * (xx + yy),
    };
}

nifly::MatTransform scaleTransform(const float scale) {
    nifly::MatTransform transform;
    transform.Clear();
    transform.scale = scale;
    return transform;
}

nifly::MatTransform transformFromMatrix(const nifly::Matrix4& matrix, const float translationScale = 1.0f) {
    nifly::MatTransform transform;
    transform.translation = nifly::Vector3(matrix[3], matrix[7], matrix[11]) * translationScale;

    auto row0 = nifly::Vector3(matrix[0], matrix[1], matrix[2]);
    auto row1 = nifly::Vector3(matrix[4], matrix[5], matrix[6]);
    auto row2 = nifly::Vector3(matrix[8], matrix[9], matrix[10]);

    const auto scale = (row0.length() + row1.length() + row2.length()) / 3.0f;
    transform.scale = scale > Epsilon ? scale : 1.0f;
    transform.rotation = nifly::Matrix3(row0 / transform.scale, row1 / transform.scale, row2 / transform.scale);
    return transform;
}

bool usesSkyrimHavokScale(const nifly::NifFile* nifFile) {
    if (!nifFile) {
        return false;
    }

    const auto& version = nifFile->GetHeader().GetVersion();
    return version.File() == nifly::NiFileVersion::V20_2_0_7 && version.User() >= 12;
}

CollisionColor bodyLayerColor(const nifly::bhkWorldObject* body) {
    if (!body) {
        return LayerColors[0];
    }

    return LayerColors[body->collisionFilter.layer & 7];
}

bool sameColor(const CollisionColor& lhs, const CollisionColor& rhs) {
    return std::fabs(lhs.r - rhs.r)
           <= Epsilon
           && std::fabs(lhs.g - rhs.g)
           <= Epsilon
           && std::fabs(lhs.b - rhs.b)
           <= Epsilon
           && std::fabs(lhs.a - rhs.a)
           <= Epsilon;
}

class Builder {
public:
    explicit Builder(const nifly::NifFile* nifFile)
        : m_NifFile(nifFile)
        , m_Header(nifFile->GetHeader())
        , m_HavokScale(usesSkyrimHavokScale(nifFile) ? SkyrimBhkScale : 1.0f) {}

    CollisionGeometry build() {
        if (auto* root = m_NifFile->GetRootNode()) {
            nifly::MatTransform identity;
            identity.Clear();
            visitObject(root, identity);
        }

        if (!m_Bounds.empty()) {
            m_Geometry.bounds = nifly::BoundingSphere(m_Bounds);
        }

        return std::move(m_Geometry);
    }

private:
    class ConvexHullEdgeEmitter {
    public:
        ConvexHullEdgeEmitter(
            Builder& builder,
            const std::vector<nifly::Vector3>& vertices,
            const nifly::MatTransform& transform,
            const CollisionColor& color
        )
            : m_Builder(builder)
            , m_Vertices(vertices)
            , m_Transform(transform)
            , m_Color(color) {}

        void addEdges() {
            for (std::size_t i = 0; i + 2 < m_Vertices.size(); i++) {
                for (std::size_t j = i + 1; j + 1 < m_Vertices.size(); j++) {
                    for (std::size_t k = j + 1; k < m_Vertices.size(); k++) {
                        const auto a = m_Vertices[i];
                        const auto b = m_Vertices[j];
                        const auto c = m_Vertices[k];
                        const auto normal = (b - a).cross(c - a);
                        if (normal.length() <= Epsilon) {
                            continue;
                        }

                        if (isHullFace(a, b, c, normal)) {
                            addTriangleEdges(i, j, k);
                        }
                    }
                }
            }
        }

    private:
        [[nodiscard]] bool isHullFace(
            const nifly::Vector3& a,
            const nifly::Vector3& b,
            const nifly::Vector3& c,
            const nifly::Vector3& normal
        ) const {
            int side = 0;
            for (const auto& vertex : m_Vertices) {
                if (vertex == a || vertex == b || vertex == c) {
                    continue;
                }

                const auto distance = (vertex - a).dot(normal);
                if (std::fabs(distance) <= Epsilon) {
                    continue;
                }

                const auto vertexSide = distance > 0.0f ? 1 : -1;
                if (side != 0 && side != vertexSide) {
                    return false;
                }
                side = vertexSide;
            }

            return true;
        }

        void addTriangleEdges(const std::size_t first, const std::size_t second, const std::size_t third) {
            addEdge(first, second);
            addEdge(second, third);
            addEdge(third, first);
        }

        void addEdge(const std::size_t edgeStart, const std::size_t edgeEnd) {
            const auto minIndex = static_cast<std::uint64_t>(std::min(edgeStart, edgeEnd));
            const auto maxIndex = static_cast<std::uint64_t>(std::max(edgeStart, edgeEnd));
            const auto edgeKey = (minIndex << 32U) | maxIndex;
            if (m_HullEdges.insert(edgeKey).second) {
                m_Builder.addLine(m_Transform, m_Vertices[edgeStart], m_Vertices[edgeEnd], m_Color);
            }
        }

        Builder& m_Builder;
        const std::vector<nifly::Vector3>& m_Vertices;
        const nifly::MatTransform& m_Transform;
        const CollisionColor& m_Color;
        std::unordered_set<std::uint64_t> m_HullEdges;
    };

    void visitObject(nifly::NiAVObject* object, const nifly::MatTransform& parent) {
        if (!object) {
            return;
        }

        const auto blockId = m_NifFile->GetBlockID(object);
        if (blockId != nifly::NIF_NPOS && !m_VisitedObjects.insert(blockId).second) {
            return;
        }

        const auto world = parent.ComposeTransforms(object->GetTransformToParent());
        addLegacyCollision(object, world);
        addBhkCollision(object, world);
        addBounds(object, world);
        addMultiBound(object, world);

        if (auto* node = dynamic_cast<nifly::NiNode*>(object)) {
            for (auto childRef : node->childRefs) {
                visitObject(m_Header.GetBlock<nifly::NiAVObject>(childRef.index), world);
            }
        }
    }

    void addLegacyCollision(nifly::NiAVObject* object, const nifly::MatTransform& transform) {
        auto* collision = m_Header.GetBlock<nifly::NiCollisionObject>(object->collisionRef);
        auto* collisionData = dynamic_cast<nifly::NiCollisionData*>(collision);
        if (!collisionData) {
            return;
        }

        addBoundingVolume(collisionData->boundingVolume, transform, BoundColor);
    }

    void addBhkCollision(nifly::NiAVObject* object, const nifly::MatTransform& transform) {
        auto* collision = m_Header.GetBlock<nifly::NiCollisionObject>(object->collisionRef);
        auto* bhkCollision = dynamic_cast<nifly::bhkNiCollisionObject*>(collision);
        if (!bhkCollision) {
            return;
        }

        auto* bodyObject = m_Header.GetBlock<nifly::NiObject>(bhkCollision->bodyRef);
        auto* body = dynamic_cast<nifly::bhkWorldObject*>(bodyObject);
        if (!body) {
            return;
        }

        auto bodyTransform = scaleTransform(BodyScale);
        if (auto* rigidBodyT = dynamic_cast<nifly::bhkRigidBodyT*>(body)) {
            bodyTransform.rotation = matrixFromQuaternion(rigidBodyT->rotation);
            bodyTransform.translation = toVector3(rigidBodyT->translation) * BodyScale * m_HavokScale;
        }

        addBhkShape(
            m_Header.GetBlock<nifly::bhkShape>(body->shapeRef),
            transform.ComposeTransforms(bodyTransform),
            bodyLayerColor(body)
        );
    }

    void addBounds(nifly::NiAVObject* object, const nifly::MatTransform& transform) {
        for (auto extraDataRef : object->extraDataRefs) {
            if (auto* bound = m_Header.GetBlock<nifly::BSBound>(extraDataRef.index)) {
                addBox(transform, bound->center - bound->halfExtents, bound->center + bound->halfExtents, BoundColor);
            }
        }
    }

    void addMultiBound(nifly::NiAVObject* object, const nifly::MatTransform& transform) {
        auto* multiBoundNode = dynamic_cast<nifly::BSMultiBoundNode*>(object);
        if (!multiBoundNode) {
            return;
        }

        auto* multiBound = m_Header.GetBlock<nifly::BSMultiBound>(multiBoundNode->multiBoundRef);
        if (!multiBound) {
            return;
        }

        auto* data = m_Header.GetBlock<nifly::BSMultiBoundData>(multiBound->dataRef);
        if (auto* aabb = dynamic_cast<nifly::BSMultiBoundAABB*>(data)) {
            addBox(transform, aabb->center - aabb->halfExtent, aabb->center + aabb->halfExtent, MultiBoundColor);
        } else if (auto* obb = dynamic_cast<nifly::BSMultiBoundOBB*>(data)) {
            nifly::MatTransform obbTransform;
            obbTransform.translation = obb->center;
            obbTransform.rotation = obb->rotation;
            obbTransform.scale = 1.0f;
            addBox(transform.ComposeTransforms(obbTransform), obb->size * -1.0f, obb->size, MultiBoundColor);
        } else if (auto* sphere = dynamic_cast<nifly::BSMultiBoundSphere*>(data)) {
            addSphere(transform, sphere->center, sphere->radius, MultiBoundColor);
        }
    }

    void addBhkShape(nifly::bhkShape* shape, const nifly::MatTransform& transform, const CollisionColor& color) {
        if (!shape) {
            return;
        }

        const auto blockId = m_NifFile->GetBlockID(shape);
        if (blockId != nifly::NIF_NPOS && !m_ShapeStack.insert(blockId).second) {
            return;
        }

        if (auto* list = dynamic_cast<nifly::bhkListShape*>(shape)) {
            for (auto subShapeRef : list->subShapeRefs) {
                addBhkShape(m_Header.GetBlock<nifly::bhkShape>(subShapeRef.index), transform, color);
            }
        } else if (auto* convexList = dynamic_cast<nifly::bhkConvexListShape*>(shape)) {
            for (auto shapeRef : convexList->shapeRefs) {
                addBhkShape(m_Header.GetBlock<nifly::bhkShape>(shapeRef.index), transform, color);
            }
        } else if (auto* transformed = dynamic_cast<nifly::bhkTransformShape*>(shape)) {
            const auto localTransform = transformFromMatrix(transformed->xform, m_HavokScale);
            addBhkShape(
                m_Header.GetBlock<nifly::bhkShape>(transformed->shapeRef),
                transform.ComposeTransforms(localTransform),
                color
            );
        } else if (auto* mopp = dynamic_cast<nifly::bhkMoppBvTreeShape*>(shape)) {
            addBhkShape(m_Header.GetBlock<nifly::bhkShape>(mopp->shapeRef), transform, color);
        } else if (auto* sphereShape = dynamic_cast<nifly::bhkSphereShape*>(shape)) {
            addSphere(transform, {}, sphereShape->radius * m_HavokScale, color);
        } else if (auto* multiSphere = dynamic_cast<nifly::bhkMultiSphereShape*>(shape)) {
            for (const auto& sphere : multiSphere->spheres) {
                addSphere(transform, sphere.center * m_HavokScale, sphere.radius * m_HavokScale, color);
            }
        } else if (auto* box = dynamic_cast<nifly::bhkBoxShape*>(shape)) {
            const auto dimensions = box->dimensions * m_HavokScale;
            addBox(transform, dimensions * -1.0f, dimensions, color);
        } else if (auto* capsule = dynamic_cast<nifly::bhkCapsuleShape*>(shape)) {
            addCapsule(
                transform,
                capsule->point1 * m_HavokScale,
                capsule->point2 * m_HavokScale,
                std::max(capsule->radius1, capsule->radius2) * m_HavokScale,
                color
            );
        } else if (auto* strips = dynamic_cast<nifly::bhkNiTriStripsShape*>(shape)) {
            addNiTriStripsShape(strips, transform.ComposeTransforms(scaleTransform(1.0f / BodyScale)), color);
        } else if (auto* convex = dynamic_cast<nifly::bhkConvexVerticesShape*>(shape)) {
            addConvexVerticesShape(convex, transform, color);
        } else if (auto* packed = dynamic_cast<nifly::bhkPackedNiTriStripsShape*>(shape)) {
            addPackedNiTriStripsShape(packed, transform, color);
        } else if (auto* packedData = dynamic_cast<nifly::hkPackedNiTriStripsData*>(shape)) {
            addPackedNiTriStripsData(packedData, transform, color);
        } else if (auto* compressed = dynamic_cast<nifly::bhkCompressedMeshShape*>(shape)) {
            addCompressedMeshShape(compressed, transform, color);
        }

        if (blockId != nifly::NIF_NPOS) {
            m_ShapeStack.erase(blockId);
        }
    }

    void addNiTriStripsShape(
        nifly::bhkNiTriStripsShape* shape,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        for (auto partRef : shape->partRefs) {
            auto* data = m_Header.GetBlock<nifly::NiTriStripsData>(partRef.index);
            if (!data) {
                continue;
            }

            std::vector<nifly::Triangle> triangles;
            data->GetTriangles(triangles);
            addTriangleEdges(data->vertices, triangles, transform, color);
        }
    }

    void addPackedNiTriStripsShape(
        nifly::bhkPackedNiTriStripsShape* shape,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        addPackedNiTriStripsData(m_Header.GetBlock<nifly::hkPackedNiTriStripsData>(shape->dataRef), transform, color);
    }

    void addPackedNiTriStripsData(
        nifly::hkPackedNiTriStripsData* data,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        if (!data) {
            return;
        }

        for (const auto& triangle : data->triData) {
            addTriangleEdges(data->compressedVertData, triangle.tri, transform, color);
        }
        for (const auto& triangle : data->triNormData) {
            addTriangleEdges(data->compressedVertData, triangle.tri, transform, color);
        }
    }

    void addConvexVerticesShape(
        nifly::bhkConvexVerticesShape* shape,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        std::vector<nifly::Vector3> vertices;
        vertices.reserve(shape->verts.size());
        for (const auto& vertex : shape->verts) {
            vertices.push_back(toVector3(vertex) * m_HavokScale);
        }

        addConvexHullEdges(vertices, transform, color);
    }

    void addConvexHullEdges(
        const std::vector<nifly::Vector3>& vertices,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        ConvexHullEdgeEmitter emitter(*this, vertices, transform, color);
        emitter.addEdges();
    }

    void addCompressedMeshShape(
        nifly::bhkCompressedMeshShape* shape,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        auto* data = m_Header.GetBlock<nifly::bhkCompressedMeshShapeData>(shape->dataRef);
        if (!data) {
            return;
        }

        for (const auto& triangle : data->bigTris) {
            if (triangle.triangle1
                < data->bigVerts.size()
                && triangle.triangle2
                < data->bigVerts.size()
                && triangle.triangle3
                < data->bigVerts.size()) {
                addTriangleEdges(
                    transform,
                    toVector3(data->bigVerts[triangle.triangle1]) * m_HavokScale,
                    toVector3(data->bigVerts[triangle.triangle2]) * m_HavokScale,
                    toVector3(data->bigVerts[triangle.triangle3]) * m_HavokScale,
                    color
                );
            }
        }

        for (auto& chunk : data->chunks) {
            addCompressedMeshChunk(data, chunk, transform, color);
        }
    }

    void addCompressedMeshChunk(
        nifly::bhkCompressedMeshShapeData* data,
        nifly::bhkCMSDChunk& chunk,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        nifly::Vector3 transformTranslation;
        nifly::Matrix3 transformRotation;
        if (chunk.transformIndex < data->transforms.size()) {
            const auto& chunkTransform = data->transforms[chunk.transformIndex];
            transformTranslation = toVector3(chunkTransform.translation);
            transformRotation = matrixFromQuaternion(chunkTransform.rotation);
        }

        std::vector<nifly::Vector3> vertices(chunk.verts.size() / 3);
        for (std::size_t i = 0; i < vertices.size(); i++) {
            const auto offset = nifly::Vector3(
                                    static_cast<float>(chunk.verts[static_cast<std::uint32_t>(3 * i)]),
                                    static_cast<float>(chunk.verts[static_cast<std::uint32_t>(3 * i + 1)]),
                                    static_cast<float>(chunk.verts[static_cast<std::uint32_t>(3 * i + 2)])
                                )
                                / 1000.0f;
            vertices[i] = transformRotation
                          * ((toVector3(chunk.translation) + transformTranslation + offset) * m_HavokScale);
        }

        std::size_t offset = 0;
        for (const auto stripLength : chunk.strips) {
            if (stripLength >= 3 && offset + stripLength <= chunk.indices.size()) {
                for (std::size_t i = 0; i + 2 < stripLength; i++) {
                    addTriangleEdges(
                        vertices,
                        chunk.indices[static_cast<std::uint32_t>(offset + i)],
                        chunk.indices[static_cast<std::uint32_t>(offset + i + 1)],
                        chunk.indices[static_cast<std::uint32_t>(offset + i + 2)],
                        transform,
                        color
                    );
                }
            }
            offset += stripLength;
        }

        for (std::size_t i = offset; i + 2 < chunk.indices.size(); i += 3) {
            addTriangleEdges(
                vertices,
                chunk.indices[static_cast<std::uint32_t>(i)],
                chunk.indices[static_cast<std::uint32_t>(i + 1)],
                chunk.indices[static_cast<std::uint32_t>(i + 2)],
                transform,
                color
            );
        }
    }

    void addBoundingVolume(
        const nifly::BoundingVolume& volume,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        switch (volume.collisionType) {
            case nifly::SPHERE_BV: addSphere(transform, volume.bvSphere.center, volume.bvSphere.radius, color); break;
            case nifly::BOX_BV:
                addOrientedBox(
                    transform,
                    volume.bvBox.center,
                    volume.bvBox.axis1,
                    volume.bvBox.axis2,
                    volume.bvBox.axis3,
                    volume.bvBox.extent1,
                    volume.bvBox.extent2,
                    volume.bvBox.extent3,
                    color
                );
                break;
            case nifly::CAPSULE_BV: {
                const auto capsuleAxis = normalized(volume.bvCapsule.origin) * volume.bvCapsule.extent;
                addCapsule(
                    transform,
                    volume.bvCapsule.center - capsuleAxis,
                    volume.bvCapsule.center + capsuleAxis,
                    volume.bvCapsule.radius,
                    color
                );
                break;
            }
            case nifly::UNION_BV:
                if (volume.bvUnion) {
                    for (const auto& child : volume.bvUnion->boundingVolumes) {
                        addBoundingVolume(child, transform, color);
                    }
                }
                break;
            case nifly::HALFSPACE_BV:
                qWarning("Skipping unsupported NiCollisionData half-space bounding volume");
                break;
            default: break;
        }
    }

    void addBox(
        const nifly::MatTransform& transform,
        const nifly::Vector3& min,
        const nifly::Vector3& max,
        const CollisionColor& color
    ) {
        addBoxEdges(
            transform,
            {{
                {min.x, min.y, min.z},
                {max.x, min.y, min.z},
                {max.x, max.y, min.z},
                {min.x, max.y, min.z},
                {min.x, min.y, max.z},
                {max.x, min.y, max.z},
                {max.x, max.y, max.z},
                {min.x, max.y, max.z},
            }},
            color
        );
    }

    void addOrientedBox(
        const nifly::MatTransform& transform,
        const nifly::Vector3& center,
        const nifly::Vector3& axis1,
        const nifly::Vector3& axis2,
        const nifly::Vector3& axis3,
        const float extent1,
        const float extent2,
        const float extent3,
        const CollisionColor& color
    ) {
        const auto a = axis1 * extent1;
        const auto b = axis2 * extent2;
        const auto c = axis3 * extent3;

        addBoxEdges(
            transform,
            {{
                center - a - b - c,
                center + a - b - c,
                center + a + b - c,
                center - a + b - c,
                center - a - b + c,
                center + a - b + c,
                center + a + b + c,
                center - a + b + c,
            }},
            color
        );
    }

    void addBoxEdges(
        const nifly::MatTransform& transform,
        const std::array<nifly::Vector3, 8>& points,
        const CollisionColor& color
    ) {
        static constexpr std::array<std::array<int, 2>, 12> Edges {{
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
        }};

        for (const auto& edge : Edges) {
            addLine(transform, points[edge[0]], points[edge[1]], color);
        }
    }

    void addSphere(
        const nifly::MatTransform& transform,
        const nifly::Vector3& center,
        const float radius,
        const CollisionColor& color
    ) {
        if (radius <= Epsilon) {
            return;
        }

        addCircle(transform, center, {radius, 0.0f, 0.0f}, {0.0f, radius, 0.0f}, color);
        addCircle(transform, center, {radius, 0.0f, 0.0f}, {0.0f, 0.0f, radius}, color);
        addCircle(transform, center, {0.0f, radius, 0.0f}, {0.0f, 0.0f, radius}, color);
    }

    void addCapsule(
        const nifly::MatTransform& transform,
        const nifly::Vector3& point1,
        const nifly::Vector3& point2,
        const float radius,
        const CollisionColor& color
    ) {
        if (radius <= Epsilon) {
            addLine(transform, point1, point2, color);
            return;
        }

        const auto axis = point2 - point1;
        if (axis.length() <= Epsilon) {
            addSphere(transform, point1, radius, color);
            return;
        }

        const auto direction = normalized(axis);
        const auto seed = std::fabs(direction.x) < 0.75f ? nifly::Vector3(1.0f, 0.0f, 0.0f)
                                                         : nifly::Vector3(0.0f, 1.0f, 0.0f);
        const auto sideA = normalized(direction.cross(seed)) * radius;
        const auto sideB = normalized(direction.cross(sideA)) * radius;

        addCircle(transform, point1, sideA, sideB, color);
        addCircle(transform, point2, sideA, sideB, color);
        addLine(transform, point1 + sideA, point2 + sideA, color);
        addLine(transform, point1 - sideA, point2 - sideA, color);
        addLine(transform, point1 + sideB, point2 + sideB, color);
        addLine(transform, point1 - sideB, point2 - sideB, color);
    }

    void addCircle(
        const nifly::MatTransform& transform,
        const nifly::Vector3& center,
        const nifly::Vector3& axisA,
        const nifly::Vector3& axisB,
        const CollisionColor& color
    ) {
        for (int i = 0; i < CircleSegments; i++) {
            const auto angleA = 2.0f * Pi * static_cast<float>(i) / static_cast<float>(CircleSegments);
            const auto angleB = 2.0f * Pi * static_cast<float>(i + 1) / static_cast<float>(CircleSegments);
            addLine(
                transform,
                center + axisA * std::cos(angleA) + axisB * std::sin(angleA),
                center + axisA * std::cos(angleB) + axisB * std::sin(angleB),
                color
            );
        }
    }

    void addTriangleEdges(
        const std::vector<nifly::Vector3>& vertices,
        const std::vector<nifly::Triangle>& triangles,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        for (const auto& triangle : triangles) {
            addTriangleEdges(vertices, triangle, transform, color);
        }
    }

    void addTriangleEdges(
        const std::vector<nifly::Vector3>& vertices,
        const nifly::Triangle& triangle,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        addTriangleEdges(vertices, triangle.p1, triangle.p2, triangle.p3, transform, color);
    }

    void addTriangleEdges(
        const std::vector<nifly::Vector3>& vertices,
        const std::uint16_t i1,
        const std::uint16_t i2,
        const std::uint16_t i3,
        const nifly::MatTransform& transform,
        const CollisionColor& color
    ) {
        if (i1 >= vertices.size() || i2 >= vertices.size() || i3 >= vertices.size() || (i1 == i2 && i2 == i3)) {
            return;
        }

        addTriangleEdges(transform, vertices[i1], vertices[i2], vertices[i3], color);
    }

    void addTriangleEdges(
        const nifly::MatTransform& transform,
        const nifly::Vector3& point1,
        const nifly::Vector3& point2,
        const nifly::Vector3& point3,
        const CollisionColor& color
    ) {
        addLine(transform, point1, point2, color);
        addLine(transform, point2, point3, color);
        addLine(transform, point3, point1, color);
    }

    void addLine(
        const nifly::MatTransform& transform,
        const nifly::Vector3& start,
        const nifly::Vector3& end,
        const CollisionColor& color
    ) {
        const auto firstVertex = m_Geometry.vertices.size();
        addVertex(transform.ApplyTransform(start), color);
        addVertex(transform.ApplyTransform(end), color);
        addLineRange(firstVertex, 2, color);
    }

    void addVertex(const nifly::Vector3& point, const CollisionColor& color) {
        m_Geometry.vertices.push_back(
            {.x = point.x, .y = point.y, .z = point.z, .r = color.r, .g = color.g, .b = color.b, .a = color.a}
        );
        m_Bounds.push_back(point);
    }

    void addLineRange(const std::size_t firstVertex, const std::size_t vertexCount, const CollisionColor& color) {
        if (!m_Geometry.lineRanges.empty()) {
            auto& range = m_Geometry.lineRanges.back();
            if (sameColor(range.color, color) && range.firstVertex + range.vertexCount == firstVertex) {
                range.vertexCount += vertexCount;
                return;
            }
        }

        m_Geometry.lineRanges.push_back({.firstVertex = firstVertex, .vertexCount = vertexCount, .color = color});
    }

    const nifly::NifFile* m_NifFile = nullptr;
    const nifly::NiHeader& m_Header;
    float m_HavokScale = 1.0f;
    CollisionGeometry m_Geometry;
    std::vector<nifly::Vector3> m_Bounds;
    std::unordered_set<std::uint32_t> m_VisitedObjects;
    std::unordered_set<std::uint32_t> m_ShapeStack;
};
} // namespace

CollisionGeometry CollisionGeometryBuilder::build(const nifly::NifFile* nifFile) {
    if (!nifFile || !nifFile->IsValid()) {
        return {};
    }

    return Builder(nifFile).build();
}
