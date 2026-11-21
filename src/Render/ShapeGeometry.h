#pragma once

#include "Nif/BoundingBox.h"
#include "Render/Resources.h"
#include "Render/ShaderManager.h"
#include "Render/ShapeSkin.h"

#include <nifly/Geometry.hpp>

#include <QMatrix4x4>
#include <QOpenGLFunctions_2_1>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace nifly {
class NifFile;
class NiShader;
class NiShape;
}

struct ObjectState;

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
    // Moves the shape for a pose: a rigid shape to where the pose has its block, a skinned one to where the pose has
    // its bones. Both this and PlaceAtRest need the GL context to move a skinned shape.
    void Place(std::span<const ObjectState> a_states, std::uint32_t a_block);
    // Back to where the shape was loaded, to the bit.
    void PlaceAtRest();
    void Draw(QOpenGLFunctions_2_1* a_f) const;
    void SetupVertexAttributes(QOpenGLFunctions_2_1* a_f) const;

    [[nodiscard]] const QMatrix4x4& ModelMatrix() const noexcept {
        return _modelMatrix;
    }
    [[nodiscard]] const nifly::BoundingSphere& Bounds() const noexcept {
        return _bounds;
    }
    [[nodiscard]] const nifly::BoundingSphere& RestBounds() const noexcept {
        return _restBounds;
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
    void Deform(std::span<const ObjectState> a_states);
    [[nodiscard]] SkinnedVertices ReadVertices() const;
    void WriteVertices(const SkinnedVertices& a_vertices) const;

    // The NIF outlives the shapes drawn from it.
    nifly::NifFile* _nifFile = nullptr;
    nifly::NiShape* _niShape = nullptr;
    // Made when a pose first moves a skinned shape.
    std::unique_ptr<ShapeSkin> _skin;
    SkinSource _skinSource;
    // A skinned shape's vertices as loaded, kept while a pose has replaced them in the buffers.
    std::optional<SkinnedVertices> _restVertices;

    OpenGLVertexArrayResource _vertexArray;
    std::array<OpenGLBufferResource, kAttribCount> _vertexBuffers;
    OpenGLBufferResource _indexBuffer;
    GLsizei _elements = 0;
    QMatrix4x4 _modelMatrix;
    nifly::BoundingSphere _bounds;
    // Around a rigid shape's vertices before the model matrix moves them.
    nifly::BoundingSphere _ownBounds;
    QMatrix4x4 _restModelMatrix;
    nifly::BoundingSphere _restBounds;
    BoundingBox _box;
    bool _skinned = false;
};
