#include "ShaderManager.h"

#include <QOpenGLContext>

ShaderManager::ShaderManager(MOBase::IOrganizer* moInfo) : m_MOInfo{moInfo}
{
}

QOpenGLShaderProgram* ShaderManager::getProgram(const ShaderType type)
{
  if (type == None) {
    return nullptr;
  }

  if (m_Programs[type] == nullptr) {
    m_Programs[type] = loadProgram(type);
  }

  return m_Programs[type];
}

QOpenGLShaderProgram* ShaderManager::loadProgram(const ShaderType type)
{
  QString vert;
  QString frag;

  switch (type) {
  case SKDefault:
    vert = "default.vert";
    frag = "sk_default.frag";
    break;
  case SKMSN:
    vert = "sk_msn.vert";
    frag = "sk_msn.frag";
    break;
  case SKMultilayer:
    vert = "default.vert";
    frag = "sk_multilayer.frag";
    break;
  case SKEffectShader:
    vert = "sk_effectshader.vert";
    frag = "sk_effectshader.frag";
    break;
  case SKPBR:
    vert = "default.vert";
    frag = "sk_pbr.frag";
    break;
  case FO4Default:
    vert = "default.vert";
    frag = "fo4_default.frag";
    break;
  case FO4EffectShader:
    vert = "default.vert";
    frag = "fo4_effectshader.frag";
    break;
  default:
    return nullptr;
  }

  const auto dataPath       = MOBase::IOrganizer::getPluginDataPath();
  const auto vertexShader   = QString("%1/shaders/%2").arg(dataPath, vert);
  const auto fragmentShader = QString("%1/shaders/%2").arg(dataPath, frag);

  const auto program = new QOpenGLShaderProgram(QOpenGLContext::currentContext());
  program->addShaderFromSourceFile(QOpenGLShader::Vertex, vertexShader);
  program->addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentShader);

  program->bindAttributeLocation("position", AttribPosition);
  program->bindAttributeLocation("normal", AttribNormal);
  program->bindAttributeLocation("tangent", AttribTangent);
  program->bindAttributeLocation("bitangent", AttribBitangent);
  program->bindAttributeLocation("texCoord", AttribTexCoord);
  program->bindAttributeLocation("color", AttribColor);

  program->link();

  return program;
}
