#pragma once

#include <QOpenGLFunctions_2_1>

class QOpenGLShaderProgram;

namespace nifly {
class NifFile;
class NiShader;
class NiShape;
}

class OpenGLShapeDrawState {
public:
    void apply(nifly::NifFile* nifFile, nifly::NiShape* niShape, nifly::NiShader* shader);
    void setupUniforms(QOpenGLShaderProgram* program) const;
    void setupOpenGLState(QOpenGLFunctions_2_1* f, bool usesBlendedPass) const;

    [[nodiscard]] bool alphaBlendEnabled() const noexcept {
        return m_AlphaBlendEnable;
    }
    [[nodiscard]] bool alphaTestEnabled() const noexcept {
        return m_AlphaTestEnable;
    }
    [[nodiscard]] bool usesAlphaPass(bool usesBlendedPass) const noexcept {
        return usesBlendedPass || m_AlphaTestEnable;
    }

private:
    void applyAlphaProperty(nifly::NifFile* nifFile, nifly::NiShape* niShape);
    void applyShaderBufferFlags(nifly::NiShader* shader);
    void setupDepthState(QOpenGLFunctions_2_1* f) const;
    void setupCullingState(QOpenGLFunctions_2_1* f) const;
    void setupBlendState(QOpenGLFunctions_2_1* f, bool usesBlendedPass) const;

    bool m_ZBufferWrite = true;
    bool m_ZBufferTest = true;
    bool m_DoubleSided = false;

    bool m_AlphaBlendEnable = false;
    GLenum m_SrcBlendMode = GL_ONE;
    GLenum m_DstBlendMode = GL_ONE;
    bool m_AlphaTestEnable = false;
    GLenum m_AlphaTestMode = GL_GREATER;
    float m_AlphaThreshold = 0.0f;
};
