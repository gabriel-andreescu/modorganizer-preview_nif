#include "Render/CollisionOverlay.h"
#include "Nif/SceneGraph.h"
#include "Render/Framing.h"
#include "Render/ShaderManager.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>

#include <limits>

OpenGLCollisionOverlay::OpenGLCollisionOverlay(const CollisionGeometry& a_geometry)
    : _lineRanges(a_geometry.lineRanges)
    , _vertexCount(a_geometry.vertices.size())
    , _bounds(a_geometry.bounds)
    , _box(a_geometry.box) {
    if (a_geometry.Empty()) {
        return;
    }
    if (_lineRanges.empty()) {
        _lineRanges.push_back({.firstVertex = 0, .vertexCount = a_geometry.vertices.size(), .color = {}});
    }
    _placements.resize(_lineRanges.size());

    auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!functions) {
        qWarning("Skipping NIF collision overlay: OpenGL 2.1 functions unavailable");
        _vertexCount = 0;
        return;
    }

    auto* const glVertexArray = _vertexArray.Create();
    glVertexArray->create();
    const auto binder = QOpenGLVertexArrayObject::Binder(glVertexArray);

    auto* const glVertexBuffer = _vertexBuffer.Create(QOpenGLBuffer::VertexBuffer);
    if (!glVertexBuffer->create() || !glVertexBuffer->bind()) {
        qWarning("Skipping NIF collision overlay: failed to create vertex buffer");
        _vertexCount = 0;
        return;
    }

    const auto byteSize = a_geometry.vertices.size() * sizeof(CollisionVertex);
    if (byteSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        qWarning("Skipping oversized NIF collision overlay vertex buffer");
        _vertexCount = 0;
        glVertexBuffer->release();
        return;
    }

    glVertexBuffer->allocate(a_geometry.vertices.data(), static_cast<int>(byteSize));

    functions->glEnableVertexAttribArray(kAttribPosition);
    functions->glVertexAttribPointer(kAttribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(CollisionVertex), nullptr);
    functions->glEnableVertexAttribArray(kAttribColor);
    functions->glVertexAttribPointer(
        kAttribColor,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(CollisionVertex),
        // OpenGL takes the offset into the bound vertex buffer as a pointer.
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        reinterpret_cast<const void*>(offsetof(CollisionVertex, r))
    );

    for (std::size_t i = 0; i < kAttribCount; i++) {
        if (i != kAttribPosition && i != kAttribColor) {
            functions->glDisableVertexAttribArray(static_cast<GLuint>(i));
        }
    }

    glVertexBuffer->release();
}

void OpenGLCollisionOverlay::Destroy() {
    _vertexBuffer.DestroyWithCurrentContext();
    _vertexArray.DestroyWithCurrentContext();

    _vertexCount = 0;
}

void OpenGLCollisionOverlay::Place(
    const std::span<const ObjectState> a_posed,
    const std::span<const ObjectState> a_rest
) {
    for (std::size_t i = 0; i < _lineRanges.size(); ++i) {
        const auto object = _lineRanges[i].object;
        _placements[i].setToIdentity();
        if (a_posed[object].posed) {
            _placements[i] = ToQMatrix(
                a_posed[object].world.ComposeTransforms(a_rest[object].world.InverseTransform())
            );
        }
    }
}

void OpenGLCollisionOverlay::PlaceAtRest() {
    for (auto& placement : _placements) {
        placement.setToIdentity();
    }
}

void OpenGLCollisionOverlay::Render(
    QOpenGLShaderProgram* a_program,
    const QMatrix4x4& a_viewMatrix,
    const QMatrix4x4& a_projectionMatrix
) const {
    if (Empty() || !a_program || !a_program->isLinked() || !a_program->bind()) {
        return;
    }

    auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!functions) {
        a_program->release();
        return;
    }

    const auto binder = QOpenGLVertexArrayObject::Binder(_vertexArray.Get());

    const auto viewProjection = a_projectionMatrix * a_viewMatrix;

    functions->glDisable(GL_POLYGON_OFFSET_FILL);
    functions->glDisable(GL_CULL_FACE);
    functions->glEnable(GL_DEPTH_TEST);
    functions->glDepthFunc(GL_LEQUAL);
    functions->glDepthMask(GL_FALSE);
    functions->glEnable(GL_BLEND);
    functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    functions->glLineWidth(1.25F);

    for (std::size_t i = 0; i < _lineRanges.size(); ++i) {
        const auto& range = _lineRanges[i];
        a_program->setUniformValue("mvpMatrix", viewProjection * _placements[i]);
        functions
            ->glDrawArrays(GL_LINES, static_cast<GLint>(range.firstVertex), static_cast<GLsizei>(range.vertexCount));
    }

    functions->glDepthMask(GL_TRUE);
    functions->glDisable(GL_BLEND);
    a_program->release();
}
