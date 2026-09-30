#include "Render/ShapeDrawState.h"
#include "Nif/ShaderFlags.h"

#include <QOpenGLShaderProgram>

#include <nifly/NifFile.hpp>

void OpenGLShapeDrawState::Apply(
    nifly::NifFile* a_nifFile,
    nifly::NiShape* a_niShape,
    nifly::NiShader* a_shader,
    const Fo4Material::Material& a_material
) {
    ApplyAlphaProperty(a_nifFile, a_niShape);
    // FO4 takes blending and alpha testing from the material, which can disagree with the NIF's alpha property.
    if (a_material.valid) {
        ApplyMaterialAlpha(a_material.alpha);
    }
    ApplyShaderBufferFlags(a_shader);

    if (a_shader) {
        _doubleSided = a_shader->IsDoubleSided();
    }
}

void OpenGLShapeDrawState::SetupUniforms(QOpenGLShaderProgram* a_program) const {
    a_program->setUniformValue("alphaThreshold", _alphaThreshold);
    a_program->setUniformValue("alphaTestMode", static_cast<GLint>(_alphaTestEnable ? _alphaTestMode : GL_ALWAYS));
    a_program->setUniformValue("doubleSided", static_cast<GLint>(_doubleSided));
}

void OpenGLShapeDrawState::SetupOpenGLState(QOpenGLFunctions_2_1* a_f, const bool a_usesBlendedPass) const {
    SetupDepthState(a_f);
    SetupCullingState(a_f);
    SetupBlendState(a_f, a_usesBlendedPass);

    if (_alphaTestEnable) {
        a_f->glDisable(GL_ALPHA_TEST);
    }
}

void OpenGLShapeDrawState::ApplyAlphaProperty(nifly::NifFile* a_nifFile, nifly::NiShape* a_niShape) {
    const auto* const alphaProperty = a_nifFile->GetAlphaProperty(a_niShape);
    if (!alphaProperty) {
        return;
    }

    const NiAlphaPropertyFlags flags = alphaProperty->flags;

    _alphaBlendEnable = flags.IsAlphaBlendEnabled();
    _srcBlendMode = flags.SourceBlendingFactor();
    _dstBlendMode = flags.DestinationBlendingFactor();
    _alphaTestEnable = flags.IsAlphaTestEnabled();
    _alphaTestMode = flags.AlphaTestMode();

    _alphaThreshold = static_cast<float>(alphaProperty->threshold) / 255.0F;
}

void OpenGLShapeDrawState::ApplyMaterialAlpha(const Fo4Material::Alpha& a_alpha) {
    _alphaBlendEnable = a_alpha.blend;
    _srcBlendMode = BlendFactor(a_alpha.sourceBlend);
    _dstBlendMode = BlendFactor(a_alpha.destinationBlend);
    _alphaTestEnable = a_alpha.test;
    _alphaTestMode = GL_GREATER;
    _alphaThreshold = static_cast<float>(a_alpha.testReference) / 255.0F;
}

void OpenGLShapeDrawState::ApplyShaderBufferFlags(nifly::NiShader* a_shader) {
    if (const auto* const bsShader = dynamic_cast<nifly::BSShaderProperty*>(a_shader)) {
        _zBufferTest = ((bsShader->shaderFlags1 & SLSF1::kZBufferTest) != 0U);
        _zBufferWrite = ((bsShader->shaderFlags2 & SLSF2::kZBufferWrite) != 0U);
    }
}

void OpenGLShapeDrawState::SetupDepthState(QOpenGLFunctions_2_1* a_f) const {
    a_f->glDepthMask(_zBufferWrite ? GL_TRUE : GL_FALSE);

    if (_zBufferTest) {
        a_f->glEnable(GL_DEPTH_TEST);
        a_f->glDepthFunc(GL_LEQUAL);
    } else {
        a_f->glDisable(GL_DEPTH_TEST);
    }
}

void OpenGLShapeDrawState::SetupCullingState(QOpenGLFunctions_2_1* a_f) const {
    if (_doubleSided) {
        a_f->glDisable(GL_CULL_FACE);
    } else {
        a_f->glEnable(GL_CULL_FACE);
        a_f->glCullFace(GL_BACK);
    }
}

void OpenGLShapeDrawState::SetupBlendState(QOpenGLFunctions_2_1* a_f, const bool a_usesBlendedPass) const {
    if (a_usesBlendedPass) {
        a_f->glDisable(GL_POLYGON_OFFSET_FILL);
        a_f->glEnable(GL_BLEND);
        if (_alphaBlendEnable) {
            a_f->glBlendFunc(_srcBlendMode, _dstBlendMode);
        } else {
            a_f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    } else {
        a_f->glDisable(GL_BLEND);
    }
}
