#include "Render/ShapeGeometry.h"
#include "Nif/ShaderFlags.h"
#include "Render/ShapeRenderGeometry.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLVertexArrayObject>

#include <nifly/NifFile.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
QMatrix4x4 ConvertTransform(const nifly::MatTransform& a_transform) {
    auto mat = a_transform.ToMatrix();
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
OpenGLBufferResource MakeVertexBuffer(const std::vector<T>* a_data, const GLuint a_attrib) {
    OpenGLBufferResource buffer;

    if (a_data && !a_data->empty()) {
        const auto byteSize = a_data->size() * sizeof(T);
        if (byteSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            qWarning("Skipping oversized vertex buffer for attribute %u", a_attrib);
            return buffer;
        }

        auto* const glBuffer = buffer.Create(QOpenGLBuffer::VertexBuffer);
        if (glBuffer->create() && glBuffer->bind()) {
            glBuffer->allocate(a_data->data(), static_cast<int>(byteSize));

            auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
                QOpenGLContext::currentContext()
            );
            if (!functions) {
                qWarning("Skipping vertex attribute setup: OpenGL 2.1 functions unavailable");
                glBuffer->release();
                return buffer;
            }

            functions->glEnableVertexAttribArray(a_attrib);

            functions
                ->glVertexAttribPointer(a_attrib, sizeof(T) / sizeof(float), GL_FLOAT, GL_FALSE, sizeof(T), nullptr);

            glBuffer->release();
        }
    }

    return buffer;
}
} // namespace

void OpenGLShapeGeometry::Initialize(nifly::NifFile* a_nifFile, nifly::NiShape* a_niShape, nifly::NiShader* a_shader) {
    auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!functions) {
        qWarning("Skipping NIF shape geometry: OpenGL 2.1 functions unavailable");
        return;
    }

    auto* const glVertexArray = _vertexArray.Create();
    glVertexArray->create();
    const auto binder = QOpenGLVertexArrayObject::Binder(glVertexArray);

    SetDefaultVertexAttributes(functions);
    ValidateShapeGeometry(a_niShape);

    const auto geometry = PrepareShapeRenderGeometry(a_nifFile, a_niShape);
    _modelMatrix = ConvertTransform(geometry.modelTransform);
    _bounds = geometry.bounds;

    if (const auto* const verts = geometry.Positions()) {
        _vertexBuffers[kAttribPosition] = MakeVertexBuffer(verts, kAttribPosition);
    }

    if (const auto* const normals = geometry.Normals()) {
        _vertexBuffers[kAttribNormal] = MakeVertexBuffer(normals, kAttribNormal);
    }

    if (const auto* const tangents = geometry.Tangents()) {
        _vertexBuffers[kAttribTangent] = MakeVertexBuffer(tangents, kAttribTangent);
    }

    if (const auto* const bitangents = geometry.Bitangents()) {
        _vertexBuffers[kAttribBitangent] = MakeVertexBuffer(bitangents, kAttribBitangent);
    }

    if (const auto* const uvs = a_nifFile->GetUvsForShape(a_niShape)) {
        _vertexBuffers[kAttribTexCoord] = MakeVertexBuffer(uvs, kAttribTexCoord);
    }

    InitializeColorBuffer(a_nifFile, a_niShape, a_shader);

    auto* const glIndexBuffer = _indexBuffer.Create(QOpenGLBuffer::IndexBuffer);
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
        _elements = static_cast<GLsizei>(
            std::min(iElements, static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()))
        );
        glIndexBuffer->release();
    }
}

void OpenGLShapeGeometry::DestroyWithCurrentContext() {
    for (auto& vertexBuffer : _vertexBuffers) {
        vertexBuffer.DestroyWithCurrentContext();
    }

    _indexBuffer.DestroyWithCurrentContext();
    _vertexArray.DestroyWithCurrentContext();
}

void OpenGLShapeGeometry::Draw(QOpenGLFunctions_2_1* a_f) const {
    if (!a_f || !_vertexArray || !_indexBuffer || !_indexBuffer->isCreated()) {
        return;
    }

    const auto binder = QOpenGLVertexArrayObject::Binder(_vertexArray.Get());
    _indexBuffer->bind();
    a_f->glDrawElements(GL_TRIANGLES, _elements, GL_UNSIGNED_SHORT, nullptr);
    _indexBuffer->release();
}

void OpenGLShapeGeometry::SetupVertexAttributes(QOpenGLFunctions_2_1* a_f) const {
    if (!_vertexArray) {
        return;
    }

    const auto binder = QOpenGLVertexArrayObject::Binder(_vertexArray.Get());
    for (std::size_t i = 0; i < kAttribCount; i++) {
        if (_vertexBuffers[i]) {
            a_f->glEnableVertexAttribArray(static_cast<GLuint>(i));
        } else {
            a_f->glDisableVertexAttribArray(static_cast<GLuint>(i));
        }
    }
}

void OpenGLShapeGeometry::SetDefaultVertexAttributes(QOpenGLFunctions_2_1* a_f) {
    a_f->glVertexAttrib2f(kAttribTexCoord, 0.0F, 0.0F);
    a_f->glVertexAttrib4f(kAttribColor, 1.0F, 1.0F, 1.0F, 1.0F);
}

void OpenGLShapeGeometry::InitializeColorBuffer(
    nifly::NifFile* a_nifFile,
    nifly::NiShape* a_niShape,
    nifly::NiShader* a_shader
) {
    std::vector<nifly::Color4> colors;
    if (!a_nifFile->GetColorsForShape(a_niShape, colors)) {
        return;
    }

    const auto* const bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(a_shader);
    if (bslsp
        && (((bslsp->shaderFlags1 & SLSF1::kVertexAlpha) == 0U) || ((bslsp->shaderFlags2 & SLSF2::kTreeAnim) != 0U))) {
        for (auto& color : colors) {
            color.a = 1.0F;
        }
    }

    _vertexBuffers[kAttribColor] = MakeVertexBuffer(&colors, kAttribColor);
}
