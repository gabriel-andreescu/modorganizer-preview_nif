#include "OpenGLShapeDrawState.h"
#include "NifExtensions.h"

#include <QOpenGLShaderProgram>

#include <NifFile.hpp>

void OpenGLShapeDrawState::apply(nifly::NifFile* nifFile, nifly::NiShape* niShape, nifly::NiShader* shader) {
    applyAlphaProperty(nifFile, niShape);
    applyShaderBufferFlags(shader);

    if (shader) {
        m_DoubleSided = shader->IsDoubleSided();
    }
}

void OpenGLShapeDrawState::setupUniforms(QOpenGLShaderProgram* program) const {
    program->setUniformValue("alphaThreshold", m_AlphaThreshold);
    program->setUniformValue("alphaTestMode", static_cast<GLint>(m_AlphaTestEnable ? m_AlphaTestMode : GL_ALWAYS));
    program->setUniformValue("doubleSided", m_DoubleSided);
}

void OpenGLShapeDrawState::setupOpenGLState(QOpenGLFunctions_2_1* f, const bool usesBlendedPass) const {
    setupDepthState(f);
    setupCullingState(f);
    setupBlendState(f, usesBlendedPass);

    if (m_AlphaTestEnable) {
        f->glDisable(GL_ALPHA_TEST);
    }
}

void OpenGLShapeDrawState::applyAlphaProperty(nifly::NifFile* nifFile, nifly::NiShape* niShape) {
    auto* const alphaProperty = nifFile->GetAlphaProperty(niShape);
    if (!alphaProperty) {
        return;
    }

    const NiAlphaPropertyFlags flags = alphaProperty->flags;

    m_AlphaBlendEnable = flags.isAlphaBlendEnabled();
    m_SrcBlendMode = flags.sourceBlendingFactor();
    m_DstBlendMode = flags.destinationBlendingFactor();
    m_AlphaTestEnable = flags.isAlphaTestEnabled();
    m_AlphaTestMode = flags.alphaTestMode();

    m_AlphaThreshold = static_cast<float>(alphaProperty->threshold) / 255.0f;
}

void OpenGLShapeDrawState::applyShaderBufferFlags(nifly::NiShader* shader) {
    if (auto* const bsShader = dynamic_cast<nifly::BSShaderProperty*>(shader)) {
        m_ZBufferTest = bsShader->shaderFlags1 & SLSF1::ZBufferTest;
        m_ZBufferWrite = bsShader->shaderFlags2 & SLSF2::ZBufferWrite;
    }
}

void OpenGLShapeDrawState::setupDepthState(QOpenGLFunctions_2_1* f) const {
    f->glDepthMask(m_ZBufferWrite ? GL_TRUE : GL_FALSE);

    if (m_ZBufferTest) {
        f->glEnable(GL_DEPTH_TEST);
        f->glDepthFunc(GL_LEQUAL);
    } else {
        f->glDisable(GL_DEPTH_TEST);
    }
}

void OpenGLShapeDrawState::setupCullingState(QOpenGLFunctions_2_1* f) const {
    if (m_DoubleSided) {
        f->glDisable(GL_CULL_FACE);
    } else {
        f->glEnable(GL_CULL_FACE);
        f->glCullFace(GL_BACK);
    }
}

void OpenGLShapeDrawState::setupBlendState(QOpenGLFunctions_2_1* f, const bool usesBlendedPass) const {
    if (usesBlendedPass) {
        f->glDisable(GL_POLYGON_OFFSET_FILL);
        f->glEnable(GL_BLEND);
        if (m_AlphaBlendEnable) {
            f->glBlendFunc(m_SrcBlendMode, m_DstBlendMode);
        } else {
            f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    } else {
        f->glDisable(GL_BLEND);
    }
}
