#pragma once

#include "TextureSlots.h"

#include <NifFile.hpp>
#include <QDir>
#include <QFileInfo>
#include <QOpenGLFunctions>
#include <QString>
#include <cstdint>
#include <cstring>

struct SLSF1
{
  SLSF1() = delete;

  enum BSLightingShaderFlags1 : std::uint32_t
  {
    Specular                = 1U << 0,
    Skinned                 = 1U << 1,
    TempRefraction          = 1U << 2,
    VertexAlpha             = 1U << 3,
    GreyscaleToPaletteColor = 1U << 4,
    GreyscaleToPaletteAlpha = 1U << 5,
    UseFalloff              = 1U << 6,
    EnvironmentMapping      = 1U << 7,
    ReceiveShadows          = 1U << 8,
    CastShadows             = 1U << 9,
    FacegenDetailMap        = 1U << 10,
    Parallax                = 1U << 11,
    ModelSpaceNormals       = 1U << 12,
    NonProjectiveShadows    = 1U << 13,
    Landscape               = 1U << 14,
    Refraction              = 1U << 15,
    FireRefraction          = 1U << 16,
    EyeEnvironmentMapping   = 1U << 17,
    HairSoftLighting        = 1U << 18,
    ScreendoorAlphaFade     = 1U << 19,
    LocalmapHideSecret      = 1U << 20,
    FaceGenRGBTint          = 1U << 21,
    OwnEmit                 = 1U << 22,
    ProjectedUV             = 1U << 23,
    MultipleTextures        = 1U << 24,
    RemappableTextures      = 1U << 25,
    Decal                   = 1U << 26,
    DynamicDecal            = 1U << 27,
    ParallaxOcclusion       = 1U << 28,
    ExternalEmittance       = 1U << 29,
    SoftEffect              = 1U << 30,
    ZBufferTest             = 1U << 31,
  };
};

struct SLSF2
{
  SLSF2() = delete;

  enum BSLightingShaderFlags2 : std::uint32_t
  {
    ZBufferWrite                = 1U << 0,
    LODLandscape                = 1U << 1,
    LODObjects                  = 1U << 2,
    NoFade                      = 1U << 3,
    DoubleSided                 = 1U << 4,
    VertexColors                = 1U << 5,
    GlowMap                     = 1U << 6,
    AssumeShadowmask            = 1U << 7,
    PackedTangent               = 1U << 8,
    MultiIndexSnow              = 1U << 9,
    VertexLighting              = 1U << 10,
    UniformScale                = 1U << 11,
    FitSlope                    = 1U << 12,
    Billboard                   = 1U << 13,
    NoLODLandBlend              = 1U << 14,
    EnvMapLightFade             = 1U << 15,
    Wireframe                   = 1U << 16,
    WeaponBlood                 = 1U << 17,
    HideOnLocalMap              = 1U << 18,
    PremultAlpha                = 1U << 19,
    CloudLOD                    = 1U << 20,
    AnisotropicLighting         = 1U << 21,
    NoTransparencyMultisampling = 1U << 22,
    // Unused01
    PBR = 1U << 23,
    // Use Multilayer (inner-layer) Map in non-PBR. Use Two Layer shading in PBR.
    MultiLayerParallax = 1U << 24,
    // Use Soft Lighting Map in non-PBR. Fuzz model in PBR. Parallax between layers in
    // Two Layer model in PBR.
    SoftLighting = 1U << 25,
    // Use Rim Lighting Map in non-PBR. Use Subsurface shading in PBR.
    RimLighting = 1U << 26,
    // Use Back Lighting Map in non-PBR. Use Marschner hair shading model in PBR. Use
    // coat normal map Two Layer model in PBR.
    BackLighting = 1U << 27,
    Unused02     = 1U << 28,
    TreeAnim     = 1U << 29,
    // Enables coat diffuse contribution in Two Layer model in PBR.
    EffectLighting = 1U << 30,
    HDLODObjects   = 1U << 31,
  };
};

struct TriShape
{
  TriShape() = delete;

  enum NiAVObjectFlags : std::uint32_t
  {
    Hidden                            = 1U << 0,
    SelectiveUpdate                   = 1U << 1,
    SelectiveUpdateTransforms         = 1U << 2,
    SelectiveUpdateController         = 1U << 3,
    SelectiveUpdateRigid              = 1U << 4,
    DisplayUIObject                   = 1U << 5,
    DisableSorting                    = 1U << 6,
    SelectiveUpdateTransformsOverride = 1U << 7,
    SaveExternalGeomData              = 1U << 9,
    NoDecals                          = 1U << 10,
    AlwaysDraw                        = 1U << 11,
    MeshLODFO4                        = 1U << 12,
    FixedBound                        = 1U << 13,
    TopFadeNode                       = 1U << 14,
    IgnoreFade                        = 1U << 15,
    NoAnimSyncX                       = 1U << 16,
    NoAnimSyncY                       = 1U << 17,
    NoAnimSyncZ                       = 1U << 18,
    NoAnimSyncS                       = 1U << 19,
    NoDismember                       = 1U << 20,
    NoDismemberValidity               = 1U << 21,
    RenderUse                         = 1U << 22,
    MaterialsApplied                  = 1U << 23,
    HighDetail                        = 1U << 24,
    ForceUpdate                       = 1U << 25,
    PreProcessedNode                  = 1U << 26,
    MeshLODSkyrim                     = 1U << 27,
  };
};

inline bool HasFlag(const std::uint32_t flags, const std::uint32_t flag)
{
  return (flags & flag) != 0;
}

struct NiAlphaPropertyFlags
{
public:
  NiAlphaPropertyFlags(const std::uint16_t flags = 0)  // NOLINT
  {
    std::memcpy(this, &flags, sizeof(NiAlphaPropertyFlags));
  }

  [[nodiscard]] bool isAlphaBlendEnabled() const { return m_AlphaBlendEnable; }

  [[nodiscard]] GLenum sourceBlendingFactor() const
  {
    return getBlendMode(m_SrcBlendMode);
  }

  [[nodiscard]] GLenum destinationBlendingFactor() const
  {
    return getBlendMode(m_DstBlendMode);
  }

  [[nodiscard]] bool isAlphaTestEnabled() const { return m_AlphaTestEnable; }

  [[nodiscard]] GLenum alphaTestMode() const { return getTestMode(m_AlphaTestMode); }

  [[nodiscard]] bool isTriangleSortDisabled() const { return m_NoSort; }

private:
  static std::uint32_t getBlendMode(const std::uint16_t flags)
  {
    switch (flags) {
    case 0:
      return GL_ONE;
    case 1:
      return GL_ZERO;
    case 2:
      return GL_SRC_COLOR;
    case 3:
      return GL_ONE_MINUS_SRC_COLOR;
    case 4:
      return GL_DST_COLOR;
    case 5:
      return GL_ONE_MINUS_DST_COLOR;
    case 6:
      return GL_SRC_ALPHA;
    case 7:
      return GL_ONE_MINUS_SRC_ALPHA;
    case 8:
      return GL_DST_ALPHA;
    case 9:
      return GL_ONE_MINUS_DST_ALPHA;
    default:
      return GL_ONE;
    };
  }

  static std::uint32_t getTestMode(const std::uint16_t flags)
  {
    switch (flags) {
    case 0:
      return GL_ALWAYS;
    case 1:
      return GL_LESS;
    case 2:
      return GL_EQUAL;
    case 3:
      return GL_LEQUAL;
    case 4:
      return GL_GREATER;
    case 5:
      return GL_NOTEQUAL;
    case 6:
      return GL_GEQUAL;
    case 7:
      return GL_NEVER;
    default:
      return GL_ALWAYS;
    }
  };

  std::uint16_t m_AlphaBlendEnable : 1;
  std::uint16_t m_SrcBlendMode : 4;
  std::uint16_t m_DstBlendMode : 4;
  std::uint16_t m_AlphaTestEnable : 1;
  std::uint16_t m_AlphaTestMode : 3;
  std::uint16_t m_NoSort : 1;
};

static_assert(sizeof(NiAlphaPropertyFlags) == 2);

inline nifly::MatTransform GetObjectTransformToGlobal(const nifly::NifFile* nifFile,
                                                      nifly::NiAVObject* object)
{
  nifly::MatTransform xform = object->GetTransformToParent();
  nifly::NiNode* parent     = nifFile->GetParentNode(object);
  while (parent) {
    xform  = parent->GetTransformToParent().ComposeTransforms(xform);
    parent = nifFile->GetParentNode(parent);
  }

  return xform;
}

inline nifly::MatTransform GetShapeTransformToGlobal(const nifly::NifFile* nifFile,
                                                     nifly::NiShape* niShape)
{
  return GetObjectTransformToGlobal(nifFile, niShape);
}

inline QString GetShaderTexturePath(nifly::BSShaderTextureSet* textureSet,
                                    const std::size_t slot)
{
  if (!textureSet || slot >= textureSet->textures.size()) {
    return {};
  }

  return QDir::fromNativeSeparators(
             QString::fromStdString(
                 textureSet->textures[static_cast<std::uint32_t>(slot)].get()))
      .trimmed();
}

inline bool TexturePathsEqual(const QString& left, const QString& right)
{
  return QString::compare(left, right, Qt::CaseInsensitive) == 0;
}

inline bool IsPBRLightingShader(const nifly::NiShader* shader)
{
  const auto bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(shader);
  return bslsp && HasFlag(bslsp->shaderFlags2, SLSF2::PBR);
}

inline bool IsNormalLikeTexturePath(const QString& texturePath)
{
  const auto stem = QFileInfo(texturePath).completeBaseName().toLower();
  return stem.endsWith("_n") || stem.endsWith("_msn");
}

inline bool IsRefractionDistortionProxy(const nifly::NifFile* nifFile,
                                        nifly::NiShape* niShape)
{
  if (!nifFile || !niShape || nifFile->GetAlphaProperty(niShape)) {
    return false;
  }

  const auto shader =
      dynamic_cast<nifly::BSLightingShaderProperty*>(nifFile->GetShader(niShape));
  if (!shader ||
      !(shader->shaderFlags1 & (SLSF1::Refraction | SLSF1::FireRefraction)) ||
      !shader->HasTextureSet()) {
    return false;
  }

  const auto textureSet    = nifFile->GetHeader().GetBlock(shader->TextureSetRef());
  const auto baseTexture   = GetShaderTexturePath(textureSet, TextureSlot::BaseMap);
  const auto normalTexture = GetShaderTexturePath(textureSet, TextureSlot::NormalMap);

  return !baseTexture.isEmpty() &&
         ((!normalTexture.isEmpty() && TexturePathsEqual(baseTexture, normalTexture)) ||
          IsNormalLikeTexturePath(baseTexture));
}

inline bool GetNodeTransformToAncestor(const nifly::NifFile* nifFile,
                                       nifly::NiNode* node,
                                       const std::uint32_t ancestorId,
                                       nifly::MatTransform& transform)
{
  transform.Clear();

  while (node) {
    const auto nodeId = nifFile->GetBlockID(node);
    if (nodeId == ancestorId) {
      return true;
    }

    transform = node->GetTransformToParent().ComposeTransforms(transform);
    node      = nifFile->GetParentNode(node);
  }

  return false;
}

inline nifly::BoundingSphere GetBoundingSphere(nifly::NifFile* nifFile,
                                               nifly::NiShape* niShape)
{
  if (const auto vertices = nifFile->GetVertsForShape(niShape)) {
    auto bounds = nifly::BoundingSphere(*vertices);

    const auto xform = GetShapeTransformToGlobal(nifFile, niShape);

    bounds.center = xform.ApplyTransform(bounds.center);
    bounds.radius = xform.ApplyTransformToDist(bounds.radius);
    return bounds;
  }

  return {};
}
