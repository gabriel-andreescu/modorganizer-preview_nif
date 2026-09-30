#pragma once

#include <QOpenGLFunctions>

#include <cstdint>
#include <cstring>

struct SLSF1 {
    SLSF1() = delete;

    enum BSLightingShaderFlags1 : std::uint32_t {
        kSpecular = 1U << 0U,
        kSkinned = 1U << 1U,
        kTempRefraction = 1U << 2U,
        kVertexAlpha = 1U << 3U,
        kGreyscaleToPaletteColor = 1U << 4U,
        kGreyscaleToPaletteAlpha = 1U << 5U,
        kUseFalloff = 1U << 6U,
        kEnvironmentMapping = 1U << 7U,
        kReceiveShadows = 1U << 8U,
        kCastShadows = 1U << 9U,
        kFacegenDetailMap = 1U << 10U,
        kParallax = 1U << 11U,
        kModelSpaceNormals = 1U << 12U,
        kNonProjectiveShadows = 1U << 13U,
        kLandscape = 1U << 14U,
        kRefraction = 1U << 15U,
        kFireRefraction = 1U << 16U,
        kEyeEnvironmentMapping = 1U << 17U,
        kHairSoftLighting = 1U << 18U,
        kScreendoorAlphaFade = 1U << 19U,
        kLocalmapHideSecret = 1U << 20U,
        kFaceGenRGBTint = 1U << 21U,
        kOwnEmit = 1U << 22U,
        kProjectedUV = 1U << 23U,
        kMultipleTextures = 1U << 24U,
        kRemappableTextures = 1U << 25U,
        kDecal = 1U << 26U,
        kDynamicDecal = 1U << 27U,
        kParallaxOcclusion = 1U << 28U,
        kExternalEmittance = 1U << 29U,
        kSoftEffect = 1U << 30U,
        kZBufferTest = 1U << 31U,
    };
};

struct SLSF2 {
    SLSF2() = delete;

    enum BSLightingShaderFlags2 : std::uint32_t {
        kZBufferWrite = 1U << 0U,
        kLODLandscape = 1U << 1U,
        kLODObjects = 1U << 2U,
        kNoFade = 1U << 3U,
        kDoubleSided = 1U << 4U,
        kVertexColors = 1U << 5U,
        kGlowMap = 1U << 6U,
        kAssumeShadowmask = 1U << 7U,
        kPackedTangent = 1U << 8U,
        kMultiIndexSnow = 1U << 9U,
        kVertexLighting = 1U << 10U,
        kUniformScale = 1U << 11U,
        kFitSlope = 1U << 12U,
        kBillboard = 1U << 13U,
        kNoLODLandBlend = 1U << 14U,
        kEnvMapLightFade = 1U << 15U,
        kWireframe = 1U << 16U,
        kWeaponBlood = 1U << 17U,
        kHideOnLocalMap = 1U << 18U,
        kPremultAlpha = 1U << 19U,
        kCloudLOD = 1U << 20U,
        kAnisotropicLighting = 1U << 21U,
        kNoTransparencyMultisampling = 1U << 22U,
        // Unused01
        kPBR = 1U << 23U,
        // Use Multilayer (inner-layer) Map in non-PBR. Use Two Layer shading in PBR.
        kMultiLayerParallax = 1U << 24U,
        // Use Soft Lighting Map in non-PBR. Fuzz model in PBR. Parallax between layers in
        // Two Layer model in PBR.
        kSoftLighting = 1U << 25U,
        // Use Rim Lighting Map in non-PBR. Use Subsurface shading in PBR.
        kRimLighting = 1U << 26U,
        // Use Back Lighting Map in non-PBR. Use Marschner hair shading model in PBR. Use
        // coat normal map Two Layer model in PBR.
        kBackLighting = 1U << 27U,
        kUnused02 = 1U << 28U,
        kTreeAnim = 1U << 29U,
        // Enables coat diffuse contribution in Two Layer model in PBR.
        kEffectLighting = 1U << 30U,
        kHDLODObjects = 1U << 31U,
    };
};

struct TriShape {
    TriShape() = delete;

    enum NiAVObjectFlags : std::uint32_t {
        kHidden = 1U << 0U,
        kSelectiveUpdate = 1U << 1U,
        kSelectiveUpdateTransforms = 1U << 2U,
        kSelectiveUpdateController = 1U << 3U,
        kSelectiveUpdateRigid = 1U << 4U,
        kDisplayUIObject = 1U << 5U,
        kDisableSorting = 1U << 6U,
        kSelectiveUpdateTransformsOverride = 1U << 7U,
        kSaveExternalGeomData = 1U << 9U,
        kNoDecals = 1U << 10U,
        kAlwaysDraw = 1U << 11U,
        kMeshLODFO4 = 1U << 12U,
        kFixedBound = 1U << 13U,
        kTopFadeNode = 1U << 14U,
        kIgnoreFade = 1U << 15U,
        kNoAnimSyncX = 1U << 16U,
        kNoAnimSyncY = 1U << 17U,
        kNoAnimSyncZ = 1U << 18U,
        kNoAnimSyncS = 1U << 19U,
        kNoDismember = 1U << 20U,
        kNoDismemberValidity = 1U << 21U,
        kRenderUse = 1U << 22U,
        kMaterialsApplied = 1U << 23U,
        kHighDetail = 1U << 24U,
        kForceUpdate = 1U << 25U,
        kPreProcessedNode = 1U << 26U,
        kMeshLODSkyrim = 1U << 27U,
    };
};

inline bool HasFlag(const std::uint32_t a_flags, const std::uint32_t a_flag) {
    return (a_flags & a_flag) != 0;
}

// Maps NiAlphaProperty's blend factor numbering, which FO4 materials share.
inline GLenum BlendFactor(const std::uint32_t a_factor) {
    switch (a_factor) {
        case 0:  return GL_ONE;
        case 1:  return GL_ZERO;
        case 2:  return GL_SRC_COLOR;
        case 3:  return GL_ONE_MINUS_SRC_COLOR;
        case 4:  return GL_DST_COLOR;
        case 5:  return GL_ONE_MINUS_DST_COLOR;
        case 6:  return GL_SRC_ALPHA;
        case 7:  return GL_ONE_MINUS_SRC_ALPHA;
        case 8:  return GL_DST_ALPHA;
        case 9:  return GL_ONE_MINUS_DST_ALPHA;
        default: return GL_ONE;
    }
}

struct NiAlphaPropertyFlags {
public:
    NiAlphaPropertyFlags(const std::uint16_t flags = 0) // NOLINT
    {
        std::memcpy(this, &flags, sizeof(NiAlphaPropertyFlags));
    }

    [[nodiscard]] bool IsAlphaBlendEnabled() const {
        return _alphaBlendEnable;
    }

    [[nodiscard]] GLenum SourceBlendingFactor() const {
        return BlendFactor(_srcBlendMode);
    }

    [[nodiscard]] GLenum DestinationBlendingFactor() const {
        return BlendFactor(_dstBlendMode);
    }

    [[nodiscard]] bool IsAlphaTestEnabled() const {
        return _alphaTestEnable;
    }

    [[nodiscard]] GLenum AlphaTestMode() const {
        return GetTestMode(_alphaTestMode);
    }

    [[nodiscard]] bool IsTriangleSortDisabled() const {
        return _noSort;
    }

private:
    static std::uint32_t GetTestMode(const std::uint16_t a_flags) {
        switch (a_flags) {
            case 0:  return GL_ALWAYS;
            case 1:  return GL_LESS;
            case 2:  return GL_EQUAL;
            case 3:  return GL_LEQUAL;
            case 4:  return GL_GREATER;
            case 5:  return GL_NOTEQUAL;
            case 6:  return GL_GEQUAL;
            case 7:  return GL_NEVER;
            default: return GL_ALWAYS;
        }
    };

    std::uint16_t _alphaBlendEnable : 1;
    std::uint16_t _srcBlendMode : 4;
    std::uint16_t _dstBlendMode : 4;
    std::uint16_t _alphaTestEnable : 1;
    std::uint16_t _alphaTestMode : 3;
    std::uint16_t _noSort : 1;
};

static_assert(sizeof(NiAlphaPropertyFlags) == 2);
