#pragma once

#include "Nif/BoundingBox.h"
#include "Render/Resources.h"
#include "Render/ShaderManager.h"

#include <nifly/Geometry.hpp>

#include <QMatrix4x4>
#include <QOpenGLFunctions_2_1>

#include <array>
#include <cstdint>

namespace nifly {
class NifFile;
class NiShader;
class NiShape;
}

// What a shape's vertex color alpha means to its shader.
enum class VertexAlpha : std::uint8_t {
    kShaderFlags,
    kWind,
    kOpacity,
};

class OpenGLShapeGeometry {
public:
    void Initialize(
        nifly::NifFile* a_nifFile,
        nifly::NiShape* a_niShape,
        nifly::NiShader* a_shader,
        VertexAlpha a_vertexAlpha
    );
    void DestroyWithCurrentContext();
    void Draw(QOpenGLFunctions_2_1* a_f) const;
    void SetupVertexAttributes(QOpenGLFunctions_2_1* a_f) const;

    [[nodiscard]] const QMatrix4x4& ModelMatrix() const noexcept {
        return _modelMatrix;
    }
    [[nodiscard]] const nifly::BoundingSphere& Bounds() const noexcept {
        return _bounds;
    }
    [[nodiscard]] const BoundingBox& Box() const noexcept {
        return _box;
    }
    [[nodiscard]] bool IsSkinned() const noexcept {
        return _skinned;
    }

private:
    static void SetDefaultVertexAttributes(QOpenGLFunctions_2_1* a_f);
    void InitializeColorBuffer(
        nifly::NifFile* a_nifFile,
        nifly::NiShape* a_niShape,
        nifly::NiShader* a_shader,
        VertexAlpha a_vertexAlpha
    );

    OpenGLVertexArrayResource _vertexArray;
    std::array<OpenGLBufferResource, kAttribCount> _vertexBuffers;
    OpenGLBufferResource _indexBuffer;
    GLsizei _elements = 0;
    QMatrix4x4 _modelMatrix;
    nifly::BoundingSphere _bounds;
    BoundingBox _box;
    bool _skinned = false;
};
