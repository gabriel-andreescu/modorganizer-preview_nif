#pragma once

#include "Nif/Fo4Material.h"

#include <QOpenGLFunctions_2_1>

class QOpenGLShaderProgram;

namespace nifly {
class NifFile;
class NiShader;
class NiShape;
}

class OpenGLShapeDrawState {
public:
    void Apply(
        nifly::NifFile* a_nifFile,
        nifly::NiShape* a_niShape,
        nifly::NiShader* a_shader,
        const Fo4Material::Material& a_material
    );
    void SetupUniforms(QOpenGLShaderProgram* a_program) const;
    void SetupOpenGLState(QOpenGLFunctions_2_1* a_f, bool a_usesBlendedPass) const;

    [[nodiscard]] bool AlphaBlendEnabled() const noexcept {
        return _alphaBlendEnable;
    }
    [[nodiscard]] bool AlphaTestEnabled() const noexcept {
        return _alphaTestEnable;
    }
    [[nodiscard]] bool UsesAlphaPass(bool a_usesBlendedPass) const noexcept {
        return a_usesBlendedPass || _alphaTestEnable;
    }

private:
    void ApplyAlphaProperty(nifly::NifFile* a_nifFile, nifly::NiShape* a_niShape);
    void ApplyMaterialAlpha(const Fo4Material::Alpha& a_alpha);
    void ApplyShaderBufferFlags(nifly::NiShader* a_shader);
    void SetupDepthState(QOpenGLFunctions_2_1* a_f) const;
    void SetupCullingState(QOpenGLFunctions_2_1* a_f) const;
    void SetupBlendState(QOpenGLFunctions_2_1* a_f, bool a_usesBlendedPass) const;

    bool _zBufferWrite = true;
    bool _zBufferTest = true;
    bool _doubleSided = false;

    bool _alphaBlendEnable = false;
    GLenum _srcBlendMode = GL_ONE;
    GLenum _dstBlendMode = GL_ONE;
    bool _alphaTestEnable = false;
    GLenum _alphaTestMode = GL_GREATER;
    float _alphaThreshold = 0.0F;
};
