#pragma once

#include "ShaderManager.h"
#include "TextureManager.h"
#include "TextureSlots.h"

#include <Geometry.hpp>
#include <NifFile.hpp>

#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

struct OpenGLShape
{
public:
  OpenGLShape(nifly::NifFile* nifFile, nifly::NiShape* niShape,
              TextureManager* textureManager);

  void destroy();
  void setupShaders(QOpenGLShaderProgram* program) const;
  [[nodiscard]] bool usesAlphaPass() const;
  [[nodiscard]] bool usesBlendedPass() const;

  static QVector2D convertVector2(nifly::Vector2 vector);
  static QVector3D convertVector3(nifly::Vector3 vector);
  static QColor convertColor(nifly::Color4 color);
  static QMatrix4x4 convertTransform(const nifly::MatTransform& transform);

  ShaderManager::ShaderType shaderType = ShaderManager::SKDefault;

  QOpenGLVertexArrayObject* vertexArray = nullptr;

  QOpenGLBuffer* vertexBuffers[ATTRIB_COUNT]{nullptr};

  QOpenGLBuffer* indexBuffer = nullptr;
  GLsizei elements           = 0;

  std::array<PreviewTexture*, TextureSlotCount> textures{nullptr};

  QMatrix4x4 modelMatrix;
  nifly::BoundingSphere bounds;
  QVector3D specColor{1.0f, 1.0f, 1.0f};
  float specStrength   = 1.0f;
  float specGlossiness = 1.0f;
  float fresnelPower;

  float paletteScale;

  bool hasGlowMap       = false;
  bool hasHeightMap     = false;
  bool hasSourceTexture = false;
  bool hasGreyscaleMap  = false;
  QColor glowColor      = QColorConstants::White;
  float glowMult        = 1.0f;

  float alpha = 1.0f;
  QVector3D tintColor{1.0f, 1.0f, 1.0f};

  QVector2D uvScale{1.0f, 1.0f};
  QVector2D uvOffset{0.0f, 0.0f};

  bool hasEmit        = false;
  bool hasSoftlight   = false;
  bool hasBacklight   = false;
  bool hasRimlight    = false;
  bool hasTintColor   = false;
  bool hasWeaponBlood = false;
  bool greyscaleAlpha = false;
  bool greyscaleColor = false;
  bool useFalloff     = false;

  bool doubleSided     = false;
  float softlight      = 0.3f;
  float backlightPower = 0.0f;
  float rimPower       = 2.0f;
  float subsurfaceRolloff{};
  float envReflection = 1.0f;
  QVector4D falloffParams{1.0f, 1.0f, 0.0f, 0.0f};
  float falloffDepth = 0.0f;

  QVector2D innerScale;
  float innerThickness;
  float outerRefraction;
  float outerReflection;

  bool zBufferWrite = true;
  bool zBufferTest  = true;

  bool alphaBlendEnable = false;
  GLenum srcBlendMode   = GL_ONE;
  GLenum dstBlendMode   = GL_ONE;
  bool alphaTestEnable  = false;
  GLenum alphaTestMode  = GL_GREATER;
  float alphaThreshold  = 0.0f;
};
