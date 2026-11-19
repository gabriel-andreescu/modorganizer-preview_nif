#pragma once

#include "Nif/CollisionGeometry.h"
#include "Render/Resources.h"

#include <QMatrix4x4>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>

#include <cstddef>
#include <vector>

class OpenGLCollisionOverlay {
public:
    explicit OpenGLCollisionOverlay(const CollisionGeometry& a_geometry);
    ~OpenGLCollisionOverlay() = default;

    OpenGLCollisionOverlay(const OpenGLCollisionOverlay&) = delete;
    OpenGLCollisionOverlay(OpenGLCollisionOverlay&&) = delete;
    OpenGLCollisionOverlay& operator=(const OpenGLCollisionOverlay&) = delete;
    OpenGLCollisionOverlay& operator=(OpenGLCollisionOverlay&&) = delete;

    void Destroy();
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
    std::size_t _vertexCount = 0;
    nifly::BoundingSphere _bounds;
    BoundingBox _box;
};
