#pragma once

#include "Render/ShaderManager.h"

#include <QOpenGLFunctions_2_1>
#include <QString>

#include <cstdint>
#include <memory>

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
    void SetupShaders(QOpenGLShaderProgram* a_program) const;
    void Draw(QOpenGLFunctions_2_1* a_f) const;

    [[nodiscard]] const QString& Name() const noexcept;
    [[nodiscard]] const OpenGLShapeTextures& Textures() const noexcept;
    [[nodiscard]] const QString& MaterialPath() const noexcept;
    [[nodiscard]] ShaderManager::ShaderType ShaderType() const noexcept;
    // The shader's block in the NIF, which its controllers name as their target.
    [[nodiscard]] std::uint32_t ShaderBlock() const noexcept;
    [[nodiscard]] const QMatrix4x4& ModelMatrix() const noexcept;
    [[nodiscard]] const nifly::BoundingSphere& Bounds() const noexcept;
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
    std::unique_ptr<OpenGLShapeTextures> _textures;
    std::unique_ptr<OpenGLShapeDrawState> _drawState;
    ShaderManager::ShaderType _shaderType = ShaderManager::kSKDefault;
    bool _isRefractionProxy = false;
};
