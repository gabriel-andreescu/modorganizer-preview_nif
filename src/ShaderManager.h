#pragma once

#include <QOpenGLShaderProgram>
#include <imoinfo.h>

enum VertexAttrib
{
  AttribPosition  = 0,
  AttribNormal    = 1,
  AttribTangent   = 2,
  AttribBitangent = 3,
  AttribTexCoord  = 4,
  AttribColor     = 5,

  ATTRIB_COUNT,
};

class ShaderManager
{
public:
  enum ShaderType
  {
    None = -1,
    SKDefault,
    SKMSN,
    SKMultilayer,
    SKEffectShader,
    FO4Default,
    FO4EffectShader,

    SHADER_COUNT,
  };

  explicit ShaderManager(MOBase::IOrganizer* moInfo);
  ~ShaderManager()                               = default;
  ShaderManager(const ShaderManager&)            = delete;
  ShaderManager(ShaderManager&&)                 = delete;
  ShaderManager& operator=(const ShaderManager&) = delete;
  ShaderManager& operator=(ShaderManager&&)      = delete;

  QOpenGLShaderProgram* getProgram(ShaderType type);

private:
  static QOpenGLShaderProgram* loadProgram(ShaderType type);

  MOBase::IOrganizer* m_MOInfo;
  QOpenGLShaderProgram* m_Programs[SHADER_COUNT]{nullptr};
};
