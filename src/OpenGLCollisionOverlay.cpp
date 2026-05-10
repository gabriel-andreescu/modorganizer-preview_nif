#include "OpenGLCollisionOverlay.h"
#include "ShaderManager.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>

#include <limits>

OpenGLCollisionOverlay::OpenGLCollisionOverlay(const CollisionGeometry& geometry)
    : m_LineRanges(geometry.lineRanges)
    , m_VertexCount(geometry.vertices.size())
    , m_Bounds(geometry.bounds) {
    if (geometry.empty()) {
        return;
    }
    if (m_LineRanges.empty()) {
        m_LineRanges.push_back({.firstVertex = 0, .vertexCount = geometry.vertices.size(), .color = {}});
    }

    auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!f) {
        qWarning("Skipping NIF collision overlay: OpenGL 2.1 functions unavailable");
        m_VertexCount = 0;
        return;
    }

    m_VertexArray = new QOpenGLVertexArrayObject();
    m_VertexArray->create();
    auto binder = QOpenGLVertexArrayObject::Binder(m_VertexArray);

    m_VertexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (!m_VertexBuffer->create() || !m_VertexBuffer->bind()) {
        qWarning("Skipping NIF collision overlay: failed to create vertex buffer");
        m_VertexCount = 0;
        return;
    }

    const auto byteSize = geometry.vertices.size() * sizeof(CollisionVertex);
    if (byteSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        qWarning("Skipping oversized NIF collision overlay vertex buffer");
        m_VertexCount = 0;
        m_VertexBuffer->release();
        return;
    }

    m_VertexBuffer->allocate(geometry.vertices.data(), static_cast<int>(byteSize));

    f->glEnableVertexAttribArray(AttribPosition);
    f->glVertexAttribPointer(AttribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(CollisionVertex), nullptr);
    f->glEnableVertexAttribArray(AttribColor);
    f->glVertexAttribPointer(
        AttribColor,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(CollisionVertex),
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        reinterpret_cast<const void*>(offsetof(CollisionVertex, r))
    );

    for (std::size_t i = 0; i < ATTRIB_COUNT; i++) {
        if (i != AttribPosition && i != AttribColor) {
            f->glDisableVertexAttribArray(static_cast<GLuint>(i));
        }
    }

    m_VertexBuffer->release();
}

void OpenGLCollisionOverlay::destroy() {
    if (m_VertexBuffer) {
        m_VertexBuffer->destroy();
        delete m_VertexBuffer;
        m_VertexBuffer = nullptr;
    }

    if (m_VertexArray) {
        m_VertexArray->destroy();
        m_VertexArray->deleteLater();
        m_VertexArray = nullptr;
    }

    m_VertexCount = 0;
}

void OpenGLCollisionOverlay::render(
    QOpenGLShaderProgram* program,
    const QMatrix4x4& viewMatrix,
    const QMatrix4x4& projectionMatrix
) const {
    if (empty() || !program || !program->isLinked() || !program->bind()) {
        return;
    }

    auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!f) {
        program->release();
        return;
    }

    auto binder = QOpenGLVertexArrayObject::Binder(m_VertexArray);

    program->setUniformValue("mvpMatrix", projectionMatrix * viewMatrix);

    f->glDisable(GL_POLYGON_OFFSET_FILL);
    f->glDisable(GL_CULL_FACE);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LEQUAL);
    f->glDepthMask(GL_FALSE);
    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glLineWidth(1.25f);

    for (const auto& range : m_LineRanges) {
        f->glDrawArrays(GL_LINES, static_cast<GLint>(range.firstVertex), static_cast<GLsizei>(range.vertexCount));
    }

    f->glDepthMask(GL_TRUE);
    f->glDisable(GL_BLEND);
    program->release();
}
