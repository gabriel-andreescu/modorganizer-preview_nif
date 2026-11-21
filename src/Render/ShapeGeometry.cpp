#include "Render/ShapeGeometry.h"
#include "Nif/SceneGraph.h"
#include "Nif/ShaderFlags.h"
#include "Render/Framing.h"
#include "Render/ShapeRenderGeometry.h"
#include "Render/ShapeSkin.h"

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
constexpr std::array kSkinnedAttributes {kAttribPosition, kAttribNormal, kAttribTangent, kAttribBitangent};

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

void OpenGLShapeGeometry::Initialize(
    nifly::NifFile* a_nifFile,
    nifly::NiShape* a_niShape,
    nifly::NiShader* a_shader,
    const VertexAlpha a_vertexAlpha
) {
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

    _nifFile = a_nifFile;
    _niShape = a_niShape;
    const auto geometry = PrepareShapeRenderGeometry(a_nifFile, a_niShape);
    _modelMatrix = ToQMatrix(geometry.modelTransform);
    _bounds = geometry.bounds;
    _box = geometry.box;
    _skinned = geometry.skinned;
    if (!_skinned && geometry.raw.positions != nullptr) {
        _ownBounds = nifly::BoundingSphere(*geometry.raw.positions);
    }
    _restModelMatrix = _modelMatrix;
    _restBounds = _bounds;

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

    InitializeColorBuffer(a_nifFile, a_niShape, a_shader, a_vertexAlpha);

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

void OpenGLShapeGeometry::Place(const std::span<const ObjectState> a_states, const std::uint32_t a_block) {
    if (_skinned) {
        Deform(a_states);
        return;
    }

    const auto& world = a_states[a_block].world;
    _modelMatrix = ToQMatrix(world);
    _bounds.center = world.ApplyTransform(_ownBounds.center);
    _bounds.radius = world.ApplyTransformToDist(_ownBounds.radius);
}

void OpenGLShapeGeometry::PlaceAtRest() {
    _modelMatrix = _restModelMatrix;
    _bounds = _restBounds;
    if (_restVertices) {
        WriteVertices(*_restVertices);
        _restVertices.reset();
    }
}

void OpenGLShapeGeometry::Deform(const std::span<const ObjectState> a_states) {
    if (!_skin) {
        _skin = std::make_unique<ShapeSkin>(_nifFile, _niShape);
        _skinSource = SkinSourceOf(_nifFile, _niShape);
    }
    if (!_skin->HasInfluences() || !_skin->MovedBy(a_states)) {
        PlaceAtRest();
        return;
    }

    // Until a pose overwrites them, the buffers hold the vertices the shape was loaded with.
    if (!_restVertices) {
        _restVertices = ReadVertices();
    }
    const auto vertices = _skin->Posed(_skinSource, a_states);
    WriteVertices(vertices);
    _bounds = nifly::BoundingSphere(vertices.positions);
}

SkinnedVertices OpenGLShapeGeometry::ReadVertices() const {
    SkinnedVertices vertices;
    const std::array lists {&vertices.positions, &vertices.normals, &vertices.tangents, &vertices.bitangents};
    for (std::size_t i = 0; i < lists.size(); ++i) {
        const auto& buffer = _vertexBuffers[kSkinnedAttributes[i]];
        if (!buffer || !buffer->bind()) {
            continue;
        }
        lists[i]->resize(static_cast<std::size_t>(buffer->size()) / sizeof(nifly::Vector3));
        buffer->read(0, lists[i]->data(), buffer->size());
        buffer->release();
    }
    return vertices;
}

void OpenGLShapeGeometry::WriteVertices(const SkinnedVertices& a_vertices) const {
    const std::array lists {&a_vertices.positions, &a_vertices.normals, &a_vertices.tangents, &a_vertices.bitangents};
    for (std::size_t i = 0; i < lists.size(); ++i) {
        const auto& buffer = _vertexBuffers[kSkinnedAttributes[i]];
        if (!buffer || lists[i]->empty() || !buffer->bind()) {
            continue;
        }
        buffer->write(0, lists[i]->data(), buffer->size());
        buffer->release();
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
    nifly::NiShader* a_shader,
    const VertexAlpha a_vertexAlpha
) {
    std::vector<nifly::Color4> colors;
    if (!a_nifFile->GetColorsForShape(a_niShape, colors)) {
        return;
    }

    const auto* const bslsp = dynamic_cast<nifly::BSLightingShaderProperty*>(a_shader);
    const bool flagsDropAlpha = bslsp != nullptr
                                && (((bslsp->shaderFlags1 & SLSF1::kVertexAlpha) == 0U)
                                    || ((bslsp->shaderFlags2 & SLSF2::kTreeAnim) != 0U));
    if (a_vertexAlpha == VertexAlpha::kWind || (a_vertexAlpha == VertexAlpha::kShaderFlags && flagsDropAlpha)) {
        for (auto& color : colors) {
            color.a = 1.0F;
        }
    }

    _vertexBuffers[kAttribColor] = MakeVertexBuffer(&colors, kAttribColor);
}
