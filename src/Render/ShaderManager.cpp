#include "Render/ShaderManager.h"

#include <QOpenGLContext>

ShaderManager::ShaderManager(MOBase::IOrganizer* a_moInfo)
    : _moInfo {a_moInfo} {}

QOpenGLShaderProgram* ShaderManager::GetProgram(const ShaderType a_type) {
    if (a_type == kNone) {
        return nullptr;
    }

    if (_programs[a_type] == nullptr) {
        _programs[a_type] = LoadProgram(a_type);
    }

    return _programs[a_type];
}

const QOpenGLShaderProgram* ShaderManager::LoadedProgram(const ShaderType a_type) const {
    return a_type == kNone ? nullptr : _programs[a_type];
}

QOpenGLShaderProgram* ShaderManager::LoadProgram(const ShaderType a_type) {
    QString vert;
    QString frag;

    switch (a_type) {
        case kSKDefault:
            vert = "default.vert";
            frag = "sk_default.frag";
            break;
        case kSKMSN:
            vert = "sk_msn.vert";
            frag = "sk_msn.frag";
            break;
        case kSKMultilayer:
            vert = "default.vert";
            frag = "sk_multilayer.frag";
            break;
        case kSKEffectShader:
            vert = "sk_effectshader.vert";
            frag = "sk_effectshader.frag";
            break;
        case kSKPBR:
            vert = "default.vert";
            frag = "sk_pbr.frag";
            break;
        case kSKRefractionProxy:
            vert = "default.vert";
            frag = "sk_refraction_proxy.frag";
            break;
        case kCollisionWire:
            vert = "collision_wire.vert";
            frag = "collision_wire.frag";
            break;
        case kFO4Default:
            vert = "default.vert";
            frag = "fo4_default.frag";
            break;
        case kFO4EffectShader:
            vert = "default.vert";
            frag = "fo4_effectshader.frag";
            break;
        default: return nullptr;
    }

    const auto dataPath = MOBase::IOrganizer::getPluginDataPath();
    const auto vertexShader = QString("%1/shaders/%2").arg(dataPath, vert);
    const auto fragmentShader = QString("%1/shaders/%2").arg(dataPath, frag);

    auto* const program = new QOpenGLShaderProgram(QOpenGLContext::currentContext());
    program->addShaderFromSourceFile(QOpenGLShader::Vertex, vertexShader);
    program->addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentShader);

    program->bindAttributeLocation("position", kAttribPosition);
    program->bindAttributeLocation("normal", kAttribNormal);
    program->bindAttributeLocation("tangent", kAttribTangent);
    program->bindAttributeLocation("bitangent", kAttribBitangent);
    program->bindAttributeLocation("texCoord", kAttribTexCoord);
    program->bindAttributeLocation("color", kAttribColor);

    program->link();

    return program;
}
