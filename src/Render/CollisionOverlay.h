#pragma once

#include "Nif/CollisionGeometry.h"
#include "Render/Resources.h"

#include <QMatrix4x4>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>

#include <cstddef>
#include <span>
#include <vector>

struct ObjectState;

class OpenGLCollisionOverlay {
public:
    explicit OpenGLCollisionOverlay(const CollisionGeometry& a_geometry);
    ~OpenGLCollisionOverlay() = default;

    OpenGLCollisionOverlay(const OpenGLCollisionOverlay&) = delete;
    OpenGLCollisionOverlay(OpenGLCollisionOverlay&&) = delete;
    OpenGLCollisionOverlay& operator=(const OpenGLCollisionOverlay&) = delete;
    OpenGLCollisionOverlay& operator=(OpenGLCollisionOverlay&&) = delete;

    void Destroy();
    // Moves each object's collision along with the object for a pose. a_rest has the objects where the NIF stores
    // them, which is where the collision was built.
    void Place(std::span<const ObjectState> a_posed, std::span<const ObjectState> a_rest);
    void PlaceAtRest();
    void Render(
        QOpenGLShaderProgram* a_program,
        const QMatrix4x4& a_viewMatrix,
        const QMatrix4x4& a_projectionMatrix
    ) const;

    [[nodiscard]] bool Empty() const {
        return _vertexCount == 0;
    }
    [[nodiscard]] const nifly::BoundingSphere& Bounds() const {
        return _bounds;
    }
    [[nodiscard]] const BoundingBox& Box() const {
        return _box;
    }

private:
    OpenGLVertexArrayResource _vertexArray;
    OpenGLBufferResource _vertexBuffer;
    std::vector<CollisionLineRange> _lineRanges;
    // Where a pose moved each line range from its place at rest.
    std::vector<QMatrix4x4> _placements;
    std::size_t _vertexCount = 0;
    nifly::BoundingSphere _bounds;
    BoundingBox _box;
};
