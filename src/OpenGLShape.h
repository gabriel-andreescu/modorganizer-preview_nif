#pragma once

#include "ShaderManager.h"

#include <memory>

class OpenGLShapeDrawState;
class OpenGLShapeGeometry;
class OpenGLShapeMaterial;
class OpenGLShapeTextures;
class QMatrix4x4;
class QOpenGLFunctions_2_1;
class QOpenGLShaderProgram;
class TextureManager;

namespace nifly {
struct BoundingSphere;
class NifFile;
class NiShape;
}

class OpenGLShape {
public:
    OpenGLShape(nifly::NifFile* nifFile, nifly::NiShape* niShape, TextureManager* textureManager);
    ~OpenGLShape();
    OpenGLShape(const OpenGLShape&) = delete;
    OpenGLShape(OpenGLShape&&) noexcept;
    OpenGLShape& operator=(const OpenGLShape&) = delete;
    OpenGLShape& operator=(OpenGLShape&&) noexcept;

    void destroy();
    void setupShaders(QOpenGLShaderProgram* program) const;
    void draw(QOpenGLFunctions_2_1* f) const;

    [[nodiscard]] ShaderManager::ShaderType shaderType() const noexcept;
    [[nodiscard]] const QMatrix4x4& modelMatrix() const noexcept;
    [[nodiscard]] const nifly::BoundingSphere& bounds() const noexcept;
    [[nodiscard]] bool isRefractionProxy() const noexcept;
    [[nodiscard]] float refractionStrength() const noexcept;
    [[nodiscard]] bool usesAlphaPass() const;
    [[nodiscard]] bool usesBlendedPass() const;

private:
    std::unique_ptr<OpenGLShapeGeometry> m_Geometry;
    std::unique_ptr<OpenGLShapeMaterial> m_Material;
    std::unique_ptr<OpenGLShapeTextures> m_Textures;
    std::unique_ptr<OpenGLShapeDrawState> m_DrawState;
    ShaderManager::ShaderType m_ShaderType = ShaderManager::SKDefault;
    bool m_IsRefractionProxy = false;
};
