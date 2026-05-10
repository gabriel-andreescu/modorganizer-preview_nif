#include "OpenGLShapeGeometry.h"
#include "NifExtensions.h"
#include "ShapeRenderGeometry.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLVertexArrayObject>

#include <NifFile.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
QMatrix4x4 convertTransform(const nifly::MatTransform& transform) {
    auto mat = transform.ToMatrix();
    return QMatrix4x4 {
        mat[0],
        mat[1],
        mat[2],
        mat[3],
        mat[4],
        mat[5],
        mat[6],
        mat[7],
        mat[8],
        mat[9],
        mat[10],
        mat[11],
        mat[12],
        mat[13],
        mat[14],
        mat[15],
    };
}

template <typename T>
OpenGLBufferResource makeVertexBuffer(const std::vector<T>* data, const GLuint attrib) {
    OpenGLBufferResource buffer;

    if (data && !data->empty()) {
        const auto byteSize = data->size() * sizeof(T);
        if (byteSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            qWarning("Skipping oversized vertex buffer for attribute %u", attrib);
            return buffer;
        }

        auto* const glBuffer = buffer.create(QOpenGLBuffer::VertexBuffer);
        if (glBuffer->create() && glBuffer->bind()) {
            glBuffer->allocate(data->data(), static_cast<int>(byteSize));

            auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
            if (!f) {
                qWarning("Skipping vertex attribute setup: OpenGL 2.1 functions unavailable");
                glBuffer->release();
                return buffer;
            }

            f->glEnableVertexAttribArray(attrib);

            f->glVertexAttribPointer(attrib, sizeof(T) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(T), nullptr);

            glBuffer->release();
        }
    }

    return buffer;
}
} // namespace

void OpenGLShapeGeometry::initialize(nifly::NifFile* nifFile, nifly::NiShape* niShape, nifly::NiShader* shader) {
    auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!f) {
        qWarning("Skipping NIF shape geometry: OpenGL 2.1 functions unavailable");
        return;
    }

    auto* const glVertexArray = m_VertexArray.create();
    glVertexArray->create();
    auto binder = QOpenGLVertexArrayObject::Binder(glVertexArray);

    setDefaultVertexAttributes(f);
    validateShapeGeometry(niShape);

    const auto geometry = prepareShapeRenderGeometry(nifFile, niShape);
    m_ModelMatrix = convertTransform(geometry.modelTransform);
    m_Bounds = geometry.bounds;

    if (const auto* const verts = geometry.positions()) {
        m_VertexBuffers[AttribPosition] = makeVertexBuffer(verts, AttribPosition);
    }

    if (const auto* const normals = geometry.normals()) {
        m_VertexBuffers[AttribNormal] = makeVertexBuffer(normals, AttribNormal);
    }

    if (const auto* const tangents = geometry.tangents()) {
        m_VertexBuffers[AttribTangent] = makeVertexBuffer(tangents, AttribTangent);
    }

    if (const auto* const bitangents = geometry.bitangents()) {
        m_VertexBuffers[AttribBitangent] = makeVertexBuffer(bitangents, AttribBitangent);
    }

    if (const auto* const uvs = nifFile->GetUvsForShape(niShape)) {
        m_VertexBuffers[AttribTexCoord] = makeVertexBuffer(uvs, AttribTexCoord);
    }

    initializeColorBuffer(nifFile, niShape, shader);

    auto* const glIndexBuffer = m_IndexBuffer.create(QOpenGLBuffer::IndexBuffer);
    if (glIndexBuffer->create() && glIndexBuffer->bind()) {
        if (!geometry.triangles.empty()) {
            const auto byteSize = geometry.triangles.size() * sizeof(nifly::Triangle);
            if (byteSize <= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                glIndexBuffer->allocate(geometry.triangles.data(), static_cast<int>(byteSize));
            } else {
                qWarning("Skipping oversized index buffer");
            }
        }

        const auto iElements = static_cast<std::uint32_t>(
            std::min<std::size_t>(geometry.triangles.size() * 3, std::numeric_limits<std::uint32_t>::max())
        );
        m_Elements = static_cast<GLsizei>(
            std::min(iElements, static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()))
        );
        glIndexBuffer->release();
    }
}

void OpenGLShapeGeometry::destroyWithCurrentContext() {
    for (auto& vertexBuffer : m_VertexBuffers) {
        vertexBuffer.destroyWithCurrentContext();
    }

    m_IndexBuffer.destroyWithCurrentContext();
    m_VertexArray.destroyWithCurrentContext();
}

void OpenGLShapeGeometry::draw(QOpenGLFunctions_2_1* f) const {
    if (!f || !m_VertexArray || !m_IndexBuffer || !m_IndexBuffer->isCreated()) {
        return;
    }

    auto binder = QOpenGLVertexArrayObject::Binder(m_VertexArray.get());
    m_IndexBuffer->bind();
    f->glDrawElements(GL_TRIANGLES, m_Elements, GL_UNSIGNED_SHORT, nullptr);
    m_IndexBuffer->release();
}

void OpenGLShapeGeometry::setupVertexAttributes(QOpenGLFunctions_2_1* f) const {
    if (!m_VertexArray) {
        return;
    }

    auto binder = QOpenGLVertexArrayObject::Binder(m_VertexArray.get());
    for (std::size_t i = 0; i < ATTRIB_COUNT; i++) {
        if (m_VertexBuffers[i]) {
            f->glEnableVertexAttribArray(static_cast<GLuint>(i));
        } else {
            f->glDisableVertexAttribArray(static_cast<GLuint>(i));
        }
    }
}

void OpenGLShapeGeometry::setDefaultVertexAttributes(QOpenGLFunctions_2_1* f) {
    f->glVertexAttrib2f(AttribTexCoord, 0.0f, 0.0f);
    f->glVertexAttrib4f(AttribColor, 1.0f, 1.0f, 1.0f, 1.0f);
}

void OpenGLShapeGeometry::initializeColorBuffer(
    nifly::NifFile* nifFile,
    nifly::NiShape* niShape,
    nifly::NiShader* shader
) {
    std::vector<nifly::Color4> colors;
    if (!nifFile->GetColorsForShape(niShape, colors)) {
        return;
    }

    if (auto* const bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(shader)) {
        if (!(bslsp->shaderFlags1 & SLSF1::VertexAlpha) || bslsp->shaderFlags2 & SLSF2::TreeAnim) {
            for (auto& color : colors) {
                color.a = 1.0f;
            }
        }
    }

    m_VertexBuffers[AttribColor] = makeVertexBuffer(&colors, AttribColor);
}
