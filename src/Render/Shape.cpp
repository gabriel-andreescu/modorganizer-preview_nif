#include "Render/Shape.h"
#include "Nif/ShaderUtils.h"
#include "Render/ShaderClassification.h"
#include "Render/ShapeDrawState.h"
#include "Render/ShapeGeometry.h"
#include "Render/ShapeMaterial.h"
#include "Render/ShapeTextures.h"
#include "Textures/Manager.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <QOpenGLVersionFunctionsFactory>

#include <nifly/NifFile.hpp>

namespace {
QString Fo4MaterialPath(const nifly::NiShader* a_shader, const ShaderManager::ShaderType a_shaderType) {
    switch (a_shaderType) {
        case ShaderManager::kFO4Default:      return GetShaderMaterialPath(a_shader, ".bgsm");
        case ShaderManager::kFO4EffectShader: return GetShaderMaterialPath(a_shader, ".bgem");
        default:                              return {};
    }
}
}

OpenGLShape::OpenGLShape(
    nifly::NifFile* a_nifFile,
    nifly::NiShape* a_niShape,
    const bool a_isGrassModel,
    TextureManager* a_textureManager
)
    : _name {QString::fromStdString(a_niShape->name.get())}
    , _geometry {std::make_unique<OpenGLShapeGeometry>()}
    , _material {std::make_unique<OpenGLShapeMaterial>()}
    , _textures {std::make_unique<OpenGLShapeTextures>()}
    , _drawState {std::make_unique<OpenGLShapeDrawState>()} {
    const auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(
        QOpenGLContext::currentContext()
    );
    if (!functions) {
        qWarning("Skipping NIF shape: OpenGL 2.1 functions unavailable");
        return;
    }

    auto* const shader = a_nifFile->GetShader(a_niShape);
    _isRefractionProxy = IsRefractionDistortionProxy(a_nifFile, a_niShape);
    _shaderType = ClassifyShaderType(a_nifFile, shader);
    const bool isPBR = _shaderType == ShaderManager::kSKPBR;
    // Fallout 4 draws grass with the model's own shader flags and material, so only Skyrim needs the grass shader's
    // vertex alpha and alpha state.
    const bool usesSkyrimGrassShader = a_isGrassModel && IsSkyrimLightingShader(_shaderType);

    _textures->InitializeDescriptors(shader, _shaderType, _isRefractionProxy);
    _geometry->Initialize(a_nifFile, a_niShape, shader, usesSkyrimGrassShader);

    if (shader) {
        _materialPath = Fo4MaterialPath(shader, _shaderType);
        const auto material = _materialPath.isEmpty() ? Fo4Material::Material {}
                                                      : a_textureManager->GetFo4Material(_materialPath);
        _textures->Load(a_nifFile, shader, _shaderType, isPBR, material, a_textureManager);
        _material->Apply(shader, isPBR, material);
        _drawState->Apply(a_nifFile, a_niShape, shader, material);
        if (usesSkyrimGrassShader) {
            _drawState->ApplySkyrimGrassAlpha(a_nifFile, a_niShape);
        }
    } else {
        _textures->UseDefaultTextures(a_textureManager);
    }
}

OpenGLShape::~OpenGLShape() = default;
OpenGLShape::OpenGLShape(OpenGLShape&&) noexcept = default;
OpenGLShape& OpenGLShape::operator=(OpenGLShape&&) noexcept = default;

void OpenGLShape::Destroy() {
    if (_geometry) {
        _geometry->DestroyWithCurrentContext();
    }
}

void OpenGLShape::SetupShaders(QOpenGLShaderProgram* a_program) const {
    _textures->SetupUniforms(a_program, _material->TextureFeatureFlags());
    _material->SetupUniforms(a_program, _shaderType);
    _drawState->SetupUniforms(a_program);

    auto* const functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(QOpenGLContext::currentContext());
    if (!functions) {
        qWarning("Skipping NIF shape shader state: OpenGL 2.1 functions unavailable");
        return;
    }

    _geometry->SetupVertexAttributes(functions);
    _drawState->SetupOpenGLState(functions, UsesBlendedPass());
}

void OpenGLShape::Draw(QOpenGLFunctions_2_1* a_f) const {
    _geometry->Draw(a_f);
}

const QString& OpenGLShape::Name() const noexcept {
    return _name;
}

const OpenGLShapeTextures& OpenGLShape::Textures() const noexcept {
    return *_textures;
}

const QString& OpenGLShape::MaterialPath() const noexcept {
    return _materialPath;
}

ShaderManager::ShaderType OpenGLShape::ShaderType() const noexcept {
    return _shaderType;
}

const QMatrix4x4& OpenGLShape::ModelMatrix() const noexcept {
    return _geometry->ModelMatrix();
}

const nifly::BoundingSphere& OpenGLShape::Bounds() const noexcept {
    return _geometry->Bounds();
}

bool OpenGLShape::IsRefractionProxy() const noexcept {
    return _isRefractionProxy;
}

float OpenGLShape::RefractionStrength() const noexcept {
    return _material->RefractionStrength();
}

bool OpenGLShape::UsesAlphaPass() const {
    return _drawState->UsesAlphaPass(UsesBlendedPass());
}

bool OpenGLShape::UsesBlendedPass() const {
    return _drawState->AlphaBlendEnabled() || _material->Alpha() < 1.0F || _material->HasRefraction();
}
