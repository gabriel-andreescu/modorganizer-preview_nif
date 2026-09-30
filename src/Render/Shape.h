#pragma once

#include "Render/ShaderManager.h"

#include <QOpenGLFunctions_2_1>
#include <QString>

#include <memory>

class OpenGLShapeDrawState;
class OpenGLShapeGeometry;
class OpenGLShapeMaterial;
class OpenGLShapeTextures;
class QMatrix4x4;
class QOpenGLShaderProgram;
class TextureManager;

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
    void SetupShaders(QOpenGLShaderProgram* a_program) const;
    void Draw(QOpenGLFunctions_2_1* a_f) const;

    [[nodiscard]] const QString& Name() const noexcept;
    [[nodiscard]] const OpenGLShapeTextures& Textures() const noexcept;
    [[nodiscard]] const QString& MaterialPath() const noexcept;
    [[nodiscard]] ShaderManager::ShaderType ShaderType() const noexcept;
    [[nodiscard]] const QMatrix4x4& ModelMatrix() const noexcept;
    [[nodiscard]] const nifly::BoundingSphere& Bounds() const noexcept;
    [[nodiscard]] bool IsRefractionProxy() const noexcept;
    [[nodiscard]] float RefractionStrength() const noexcept;
    [[nodiscard]] bool UsesAlphaPass() const;
    [[nodiscard]] bool UsesBlendedPass() const;

private:
    QString _name;
    QString _materialPath;
    std::unique_ptr<OpenGLShapeGeometry> _geometry;
    std::unique_ptr<OpenGLShapeMaterial> _material;
    std::unique_ptr<OpenGLShapeTextures> _textures;
    std::unique_ptr<OpenGLShapeDrawState> _drawState;
    ShaderManager::ShaderType _shaderType = ShaderManager::kSKDefault;
    bool _isRefractionProxy = false;
};
