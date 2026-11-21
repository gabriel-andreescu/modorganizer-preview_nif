#pragma once

#include "Render/ShaderManager.h"

#include <QOpenGLFunctions_2_1>
#include <QString>

#include <cstdint>
#include <memory>
#include <span>

namespace Animation {
struct ShaderValues;
}

class OpenGLShapeDrawState;
class OpenGLShapeGeometry;
class OpenGLShapeMaterial;
class OpenGLShapeTextures;
class QMatrix4x4;
class QOpenGLShaderProgram;
class TextureManager;
struct BoundingBox;
struct ObjectState;

namespace nifly {
struct BoundingSphere;
class NifFile;
class NiShape;
}

class OpenGLShape {
public:
    OpenGLShape(
        nifly::NifFile* a_nifFile,
        nifly::NiShape* a_niShape,
        bool a_isGrassModel,
        TextureManager* a_textureManager
    );
    ~OpenGLShape();
    OpenGLShape(const OpenGLShape&) = delete;
    OpenGLShape(OpenGLShape&&) noexcept;
    OpenGLShape& operator=(const OpenGLShape&) = delete;
    OpenGLShape& operator=(OpenGLShape&&) noexcept;

    void Destroy();
    // Draws with these animated shader values from now on. Null goes back to the shader's own values.
    void Animate(const Animation::ShaderValues* a_values);
    // Puts the shape where a pose's object states have it, and hides it when the pose does. Both this and Rest need
    // the GL context to move a skinned shape.
    void Place(std::span<const ObjectState> a_states);
    // Back to the material, place and visibility the shape was loaded with.
    void Rest();
    void SetupShaders(QOpenGLShaderProgram* a_program) const;
    void Draw(QOpenGLFunctions_2_1* a_f) const;

    [[nodiscard]] const QString& Name() const noexcept;
    [[nodiscard]] const OpenGLShapeTextures& Textures() const noexcept;
    [[nodiscard]] const QString& MaterialPath() const noexcept;
    [[nodiscard]] ShaderManager::ShaderType ShaderType() const noexcept;
    // The shader's block in the NIF, which its controllers name as their target.
    [[nodiscard]] std::uint32_t ShaderBlock() const noexcept;
    // The shape's own block in the NIF.
    [[nodiscard]] std::uint32_t Block() const noexcept;
    // Hidden shapes are kept only in a NIF whose animation can show them.
    [[nodiscard]] bool IsHidden() const noexcept;
    [[nodiscard]] bool IsHiddenAtRest() const noexcept;
    [[nodiscard]] const QMatrix4x4& ModelMatrix() const noexcept;
    [[nodiscard]] const nifly::BoundingSphere& Bounds() const noexcept;
    [[nodiscard]] const nifly::BoundingSphere& RestBounds() const noexcept;
    [[nodiscard]] const BoundingBox& Box() const noexcept;
    [[nodiscard]] bool IsSkinned() const noexcept;
    [[nodiscard]] bool IsRefractionProxy() const noexcept;
    [[nodiscard]] float RefractionStrength() const noexcept;
    [[nodiscard]] bool UsesAlphaPass() const;
    [[nodiscard]] bool UsesBlendedPass() const;

private:
    QString _name;
    QString _materialPath;
    std::unique_ptr<OpenGLShapeGeometry> _geometry;
    std::unique_ptr<OpenGLShapeMaterial> _material;
    // The material as the shader stores it, which animation starts from.
    std::unique_ptr<OpenGLShapeMaterial> _restMaterial;
    std::uint32_t _shaderBlock = UINT32_MAX;
    std::uint32_t _block = UINT32_MAX;
    bool _hiddenAtRest = false;
    bool _hidden = false;
    std::unique_ptr<OpenGLShapeTextures> _textures;
    std::unique_ptr<OpenGLShapeDrawState> _drawState;
    ShaderManager::ShaderType _shaderType = ShaderManager::kSKDefault;
    bool _isRefractionProxy = false;
};
