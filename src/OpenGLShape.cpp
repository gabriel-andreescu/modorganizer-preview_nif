#include "OpenGLShape.h"
#include "NifExtensions.h"
#include "OpenGLShapeDrawState.h"
#include "OpenGLShapeGeometry.h"
#include "OpenGLShapeMaterial.h"
#include "OpenGLShapeTextures.h"
#include "ShaderClassification.h"
#include "TextureManager.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <QOpenGLVersionFunctionsFactory>

#include <NifFile.hpp>

OpenGLShape::OpenGLShape(nifly::NifFile* nifFile, nifly::NiShape* niShape, TextureManager* textureManager)
    : m_Geometry {std::make_unique<OpenGLShapeGeometry>()}
    , m_Material {std::make_unique<OpenGLShapeMaterial>()}
    , m_Textures {std::make_unique<OpenGLShapeTextures>()}
    , m_DrawState {std::make_unique<OpenGLShapeDrawState>()} {
    auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!f) {
        qWarning("Skipping NIF shape: OpenGL 2.1 functions unavailable");
        return;
    }

    auto* const shader = nifFile->GetShader(niShape);
    m_IsRefractionProxy = IsRefractionDistortionProxy(nifFile, niShape);
    m_ShaderType = classifyShaderType(nifFile, shader);
    const bool isPBR = m_ShaderType == ShaderManager::SKPBR;

    m_Textures->initializeDescriptors(shader, m_ShaderType, m_IsRefractionProxy);
    m_Geometry->initialize(nifFile, niShape, shader);

    if (shader) {
        m_Textures->load(nifFile, shader, m_ShaderType, isPBR, textureManager);
        m_Material->apply(shader, isPBR);
        m_DrawState->apply(nifFile, niShape, shader);
    } else {
        m_Textures->useDefaultTextures(textureManager);
    }
}

OpenGLShape::~OpenGLShape() = default;
OpenGLShape::OpenGLShape(OpenGLShape&&) noexcept = default;
OpenGLShape& OpenGLShape::operator=(OpenGLShape&&) noexcept = default;

void OpenGLShape::destroy() {
    if (m_Geometry) {
        m_Geometry->destroyWithCurrentContext();
    }
}

void OpenGLShape::setupShaders(QOpenGLShaderProgram* program) const {
    m_Textures->setupUniforms(program, m_Material->textureFeatureFlags());
    m_Material->setupUniforms(program, m_ShaderType);
    m_DrawState->setupUniforms(program);

    auto* const f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!f) {
        qWarning("Skipping NIF shape shader state: OpenGL 2.1 functions unavailable");
        return;
    }

    m_Geometry->setupVertexAttributes(f);
    m_DrawState->setupOpenGLState(f, usesBlendedPass());
}

void OpenGLShape::draw(QOpenGLFunctions_2_1* f) const {
    m_Geometry->draw(f);
}

ShaderManager::ShaderType OpenGLShape::shaderType() const noexcept {
    return m_ShaderType;
}

const QMatrix4x4& OpenGLShape::modelMatrix() const noexcept {
    return m_Geometry->modelMatrix();
}

const nifly::BoundingSphere& OpenGLShape::bounds() const noexcept {
    return m_Geometry->bounds();
}

bool OpenGLShape::isRefractionProxy() const noexcept {
    return m_IsRefractionProxy;
}

float OpenGLShape::refractionStrength() const noexcept {
    return m_Material->refractionStrength();
}

bool OpenGLShape::usesAlphaPass() const {
    return m_DrawState->usesAlphaPass(usesBlendedPass());
}

bool OpenGLShape::usesBlendedPass() const {
    return m_DrawState->alphaBlendEnabled() || m_Material->alpha() < 1.0f || m_Material->hasRefraction();
}
