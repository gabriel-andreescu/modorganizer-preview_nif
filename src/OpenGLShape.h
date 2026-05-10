#pragma once

#include "ShaderManager.h"
#include "TextureManager.h"
#include "TextureSlots.h"

#include <Geometry.hpp>
#include <NifFile.hpp>

#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <cstdint>

class QOpenGLFunctions_2_1;

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

  bool hasEmit           = false;
  bool hasSoftlight      = false;
  bool hasBacklight      = false;
  bool hasRimlight       = false;
  bool hasTintColor      = false;
  bool hasWeaponBlood    = false;
  bool hasRefraction     = false;
  bool isRefractionProxy = false;
  bool greyscaleAlpha    = false;
  bool greyscaleColor    = false;
  bool useFalloff        = false;

  bool doubleSided     = false;
  float softlight      = 0.3f;
  float backlightPower = 0.0f;
  float rimPower       = 2.0f;
  float subsurfaceRolloff{};
  float envReflection = 1.0f;
  QVector4D falloffParams{1.0f, 1.0f, 0.0f, 0.0f};
  float falloffDepth       = 0.0f;
  float refractionStrength = 0.0f;

  QVector2D innerScale;
  float innerThickness;
  float outerRefraction;
  float outerReflection;

  bool isPBR                    = false;
  bool pbrHasEmissive           = false;
  bool pbrHasDisplacement       = false;
  bool pbrHasFeaturesTexture0   = false;
  bool pbrHasFeaturesTexture1   = false;
  bool pbrHasSubsurface         = false;
  bool pbrHasTwoLayer           = false;
  bool pbrHasColoredCoat        = false;
  bool pbrHasInterlayerParallax = false;
  bool pbrHasCoatNormal         = false;
  bool pbrHasFuzz               = false;
  bool pbrHasHairMarschner      = false;
  bool pbrHasGlint              = false;
  QVector3D pbrParams1{1.0f, 1.0f, 0.04f};
  QVector4D pbrParams2{1.0f, 1.0f, 1.0f, 0.0f};
  QVector4D pbrFeatureParams{0.0f, 0.0f, 0.0f, 0.0f};

  bool zBufferWrite = true;
  bool zBufferTest  = true;

  bool alphaBlendEnable = false;
  GLenum srcBlendMode   = GL_ONE;
  GLenum dstBlendMode   = GL_ONE;
  bool alphaTestEnable  = false;
  GLenum alphaTestMode  = GL_GREATER;
  float alphaThreshold  = 0.0f;

private:
  void configureShaderType(nifly::NifFile* nifFile, nifly::NiShader* shader);
  static void setDefaultVertexAttributes(QOpenGLFunctions_2_1* f);
  void initializeGeometryBuffers(nifly::NifFile* nifFile, nifly::NiShape* niShape,
                                 nifly::NiShader* shader);
  void initializeColorBuffer(nifly::NifFile* nifFile, nifly::NiShape* niShape,
                             nifly::NiShader* shader);
  void loadShaderTextures(nifly::NifFile* nifFile, nifly::NiShader* shader,
                          TextureManager* textureManager,
                          std::array<bool, TextureSlotCount>& loadedTextureSlots);
  void loadEffectShaderTextures(nifly::BSEffectShaderProperty* shader,
                                TextureManager* textureManager);
  void loadTextureSetTextures(nifly::NifFile* nifFile, nifly::NiShader* shader,
                              TextureManager* textureManager,
                              std::array<bool, TextureSlotCount>& loadedTextureSlots);
  void assignMissingTexture(TextureManager* textureManager, nifly::NiShader* shader,
                            std::size_t textureSlot);
  void assignMissingPBRTexture(TextureManager* textureManager, std::size_t textureSlot);
  void assignMissingStandardTexture(TextureManager* textureManager,
                                    nifly::NiShader* shader,
                                    std::size_t textureSlot);
  void applyShaderMaterial(nifly::NifFile* nifFile, nifly::NiShape* niShape,
                           nifly::NiShader* shader,
                           const std::array<bool, TextureSlotCount>& loadedTextureSlots);
  void applyCommonShaderMaterial(nifly::NiShader* shader);
  void applyAlphaProperty(nifly::NifFile* nifFile, nifly::NiShape* niShape);
  void applyShaderBufferFlags(nifly::NiShader* shader);
  void applyLightingShaderMaterial(
      nifly::BSLightingShaderProperty* shader,
      const std::array<bool, TextureSlotCount>& loadedTextureSlots);
  void applyEffectShaderMaterial(nifly::BSEffectShaderProperty* shader);
  void useDefaultTextures(TextureManager* textureManager);
  void bindTextures() const;
  void setupGlowUniforms(QOpenGLShaderProgram* program) const;
  void setupPBRUniforms(QOpenGLShaderProgram* program) const;
  void setupMultilayerUniforms(QOpenGLShaderProgram* program) const;
  void setupVertexAttributes(QOpenGLFunctions_2_1* f) const;
  void setupDepthState(QOpenGLFunctions_2_1* f) const;
  void setupCullingState(QOpenGLFunctions_2_1* f) const;
  void setupBlendState(QOpenGLFunctions_2_1* f) const;
};
