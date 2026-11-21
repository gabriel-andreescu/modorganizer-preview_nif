#include "Textures/SlotDescriptors.h"
#include "Nif/ShaderUtils.h"

#include <QObject>

namespace {
using SamplerUniforms = std::array<TextureSamplerUniform, 2>;
using FeatureUniforms = std::array<TextureFeatureUniform, 2>;

bool IsEffectShaderType(const ShaderManager::ShaderType a_shaderType) {
    return a_shaderType == ShaderManager::kSKEffectShader || a_shaderType == ShaderManager::kFO4EffectShader;
}

bool IsMultilayerShader(const nifly::NiShader* a_shader) {
    const auto* const bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(a_shader);
    return (bslsp != nullptr) && bslsp->GetShaderType() == nifly::BSLSP_MULTILAYERPARALLAX;
}

bool UsesHeightSlot(const nifly::NiShader* a_shader) {
    const auto* const bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(a_shader);
    if (!bslsp) {
        return false;
    }

    const auto shaderType = bslsp->GetShaderType();
    return shaderType == nifly::BSLSP_PARALLAX || shaderType == nifly::BSLSP_PARALLAXOCC;
}

TextureSlotFamily FamilyForSlot(
    const nifly::NiShader* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    const std::size_t a_slot,
    const bool a_isRefractionProxy
) {
    if ((a_isRefractionProxy || a_shaderType == ShaderManager::kSKRefractionProxy) && a_slot == TextureSlot::kBaseMap) {
        return TextureSlotFamily::kRefractionProxy;
    }
    if (a_shaderType == ShaderManager::kSKPBR) {
        return TextureSlotFamily::kPBR;
    }
    if (IsEffectShaderType(a_shaderType)) {
        return TextureSlotFamily::kEffect;
    }
    if (a_shaderType == ShaderManager::kNone) {
        if (IsPBRLightingShader(a_shader)) {
            return TextureSlotFamily::kPBR;
        }
        if (dynamic_cast<const nifly::BSEffectShaderProperty*>(a_shader)) {
            return TextureSlotFamily::kEffect;
        }
    }

    return TextureSlotFamily::kStandard;
}

TextureSlotRole StandardTextureSlotRole(
    const nifly::NiShader* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    const std::size_t a_slot
) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:   return TextureSlotRole::kBase;
        case TextureSlot::kNormalMap: return TextureSlotRole::kNormal;
        case TextureSlot::kGlowMap:
            return a_shader && a_shader->HasGlowmap() ? TextureSlotRole::kGlow : TextureSlotRole::kLight;
        case TextureSlot::kHeightMap:
            if (a_shaderType == ShaderManager::kFO4Default) {
                return TextureSlotRole::kGreyscale;
            }
            if (UsesHeightSlot(a_shader)) {
                return TextureSlotRole::kHeight;
            }
            if (a_shaderType == ShaderManager::kSKMultilayer) {
                return TextureSlotRole::kNumbered;
            }
            return TextureSlotRole::kDetail;
        case TextureSlot::kEnvironmentMap:  return TextureSlotRole::kEnvironment;
        case TextureSlot::kEnvironmentMask: return TextureSlotRole::kEnvironmentMask;
        case TextureSlot::kTintMask:
            return IsMultilayerShader(a_shader) ? TextureSlotRole::kInner : TextureSlotRole::kTint;
        case TextureSlot::kBacklightMap:
            return a_shader && a_shader->HasBacklight() ? TextureSlotRole::kBacklight : TextureSlotRole::kSpecular;
        default: return TextureSlotRole::kNumbered;
    }
}

TextureSlotRole EffectTextureSlotRole(const ShaderManager::ShaderType a_shaderType, const std::size_t a_slot) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:      return TextureSlotRole::kBase;
        case TextureSlot::kGreyscaleMap: return TextureSlotRole::kGreyscale;
        case TextureSlot::kNormalMap:
            return a_shaderType == ShaderManager::kFO4EffectShader ? TextureSlotRole::kNormal
                                                                   : TextureSlotRole::kNumbered;
        case TextureSlot::kEnvironmentMap:
            return a_shaderType == ShaderManager::kFO4EffectShader ? TextureSlotRole::kEnvironment
                                                                   : TextureSlotRole::kNumbered;
        case TextureSlot::kEnvironmentMask:
            return a_shaderType == ShaderManager::kFO4EffectShader ? TextureSlotRole::kEnvironmentMask
                                                                   : TextureSlotRole::kNumbered;
        default: return TextureSlotRole::kNumbered;
    }
}

TextureSlotRole PbrTextureSlotRole(const std::size_t a_slot) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:         return TextureSlotRole::kBase;
        case TextureSlot::kNormalMap:       return TextureSlotRole::kNormal;
        case TextureSlot::kPBREmissiveMap:  return TextureSlotRole::kEmissive;
        case TextureSlot::kPBRDisplacement: return TextureSlotRole::kDisplacement;
        case TextureSlot::kPBRRMAOSMap:     return TextureSlotRole::kRMAOS;
        case TextureSlot::kPBRFeatures1:    return TextureSlotRole::kFuzzCoatNormal;
        case TextureSlot::kPBRFeatures0:    return TextureSlotRole::kSubsurfaceCoatColor;
        default:                            return TextureSlotRole::kNumbered;
    }
}

TextureSlotRole RoleForSlot(
    const nifly::NiShader* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    const std::size_t a_slot,
    const TextureSlotFamily a_family
) {
    switch (a_family) {
        case TextureSlotFamily::kRefractionProxy: return TextureSlotRole::kRefraction;
        case TextureSlotFamily::kPBR:             return PbrTextureSlotRole(a_slot);
        case TextureSlotFamily::kEffect:          return EffectTextureSlotRole(a_shaderType, a_slot);
        case TextureSlotFamily::kStandard:        return StandardTextureSlotRole(a_shader, a_shaderType, a_slot);
    }

    return TextureSlotRole::kNumbered;
}

TextureFallback PbrTextureSetFallback(const std::size_t a_slot) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:         return TextureFallback::kError;
        case TextureSlot::kNormalMap:       return TextureFallback::kFlatNormal;
        case TextureSlot::kPBREmissiveMap:
        case TextureSlot::kPBRDisplacement: return TextureFallback::kBlack;
        case TextureSlot::kPBRRMAOSMap:
        case TextureSlot::kPBRFeatures0:
        case TextureSlot::kPBRFeatures1:    return TextureFallback::kWhite;
        default:                            return TextureFallback::kNone;
    }
}

TextureFallback StandardTextureSetFallback(const nifly::NiShader* a_shader, const std::size_t a_slot) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:   return TextureFallback::kError;
        case TextureSlot::kNormalMap: return TextureFallback::kFlatNormal;
        case TextureSlot::kGlowMap:
            return a_shader && a_shader->HasGlowmap() ? TextureFallback::kBlack : TextureFallback::kWhite;
        default: return TextureFallback::kNone;
    }
}

TextureFallback TextureSetFallback(
    const nifly::NiShader* a_shader,
    const std::size_t a_slot,
    const TextureSlotFamily a_family
) {
    switch (a_family) {
        case TextureSlotFamily::kPBR: return PbrTextureSetFallback(a_slot);
        case TextureSlotFamily::kRefractionProxy:
            return a_slot == TextureSlot::kBaseMap ? TextureFallback::kError : TextureFallback::kNone;
        case TextureSlotFamily::kEffect:   return TextureFallback::kNone;
        case TextureSlotFamily::kStandard: return StandardTextureSetFallback(a_shader, a_slot);
    }

    return TextureFallback::kNone;
}

TextureFallbackPolicy FallbackPolicy(const TextureFallback a_emptyPath, const TextureFallback a_failedLoad) {
    return {.emptyPath = a_emptyPath, .failedLoad = a_failedLoad};
}

TextureFallbackPolicy EffectDirectFallbackPolicy(
    const ShaderManager::ShaderType a_shaderType,
    const std::size_t a_slot
) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:
        case TextureSlot::kGreyscaleMap: return FallbackPolicy(TextureFallback::kWhite, TextureFallback::kError);
        case TextureSlot::kNormalMap:
            return a_shaderType == ShaderManager::kFO4EffectShader
                       ? FallbackPolicy(TextureFallback::kFlatNormal, TextureFallback::kFlatNormal)
                       : TextureFallbackPolicy {};
        case TextureSlot::kEnvironmentMap:
            return a_shaderType == ShaderManager::kFO4EffectShader
                       ? FallbackPolicy(TextureFallback::kNone, TextureFallback::kNone)
                       : TextureFallbackPolicy {};
        case TextureSlot::kEnvironmentMask:
            return a_shaderType == ShaderManager::kFO4EffectShader
                       ? FallbackPolicy(TextureFallback::kWhite, TextureFallback::kError)
                       : TextureFallbackPolicy {};
        default: return {};
    }
}

TextureFallbackPolicy DirectFallbackPolicy(const ShaderManager::ShaderType a_shaderType, const std::size_t a_slot) {
    if (IsEffectShaderType(a_shaderType)) {
        return EffectDirectFallbackPolicy(a_shaderType, a_slot);
    }

    return {};
}

SamplerUniforms NoSamplerUniforms(std::size_t& a_count) {
    a_count = 0;
    return {};
}

SamplerUniforms SamplerUniform(const char* a_name, std::size_t& a_count) {
    a_count = 1;
    return {TextureSamplerUniform {.name = a_name}, {}};
}

SamplerUniforms MakeSamplerUniforms(const char* a_first, const char* a_second, std::size_t& a_count) {
    a_count = 2;
    return {TextureSamplerUniform {.name = a_first}, TextureSamplerUniform {.name = a_second}};
}

SamplerUniforms SkDefaultSamplerUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:         return SamplerUniform("BaseMap", a_count);
        case TextureSlot::kNormalMap:       return SamplerUniform("NormalMap", a_count);
        case TextureSlot::kGlowMap:         return MakeSamplerUniforms("GlowMap", "LightMask", a_count);
        case TextureSlot::kHeightMap:       return SamplerUniform("HeightMap", a_count);
        case TextureSlot::kEnvironmentMap:  return SamplerUniform("CubeMap", a_count);
        case TextureSlot::kEnvironmentMask: return SamplerUniform("EnvironmentMap", a_count);
        case TextureSlot::kBacklightMap:    return SamplerUniform("BacklightMap", a_count);
        default:                            return NoSamplerUniforms(a_count);
    }
}

SamplerUniforms SkMsnSamplerUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:      return SamplerUniform("BaseMap", a_count);
        case TextureSlot::kNormalMap:    return SamplerUniform("NormalMap", a_count);
        case TextureSlot::kLightMask:    return SamplerUniform("LightMask", a_count);
        case TextureSlot::kDetailMask:   return SamplerUniform("DetailMask", a_count);
        case TextureSlot::kTintMask:     return SamplerUniform("TintMask", a_count);
        case TextureSlot::kBacklightMap: return MakeSamplerUniforms("BacklightMap", "SpecularMap", a_count);
        default:                         return NoSamplerUniforms(a_count);
    }
}

SamplerUniforms SkMultilayerSamplerUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:         return SamplerUniform("BaseMap", a_count);
        case TextureSlot::kNormalMap:       return SamplerUniform("NormalMap", a_count);
        case TextureSlot::kLightMask:       return SamplerUniform("LightMask", a_count);
        case TextureSlot::kEnvironmentMap:  return SamplerUniform("CubeMap", a_count);
        case TextureSlot::kEnvironmentMask: return SamplerUniform("EnvironmentMap", a_count);
        case TextureSlot::kInnerMap:        return SamplerUniform("InnerMap", a_count);
        case TextureSlot::kBacklightMap:    return SamplerUniform("BacklightMap", a_count);
        default:                            return NoSamplerUniforms(a_count);
    }
}

SamplerUniforms SkEffectSamplerUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:      return SamplerUniform("BaseMap", a_count);
        case TextureSlot::kGreyscaleMap: return SamplerUniform("GreyscaleMap", a_count);
        default:                         return NoSamplerUniforms(a_count);
    }
}

SamplerUniforms SkPbrSamplerUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:         return SamplerUniform("BaseMap", a_count);
        case TextureSlot::kNormalMap:       return SamplerUniform("NormalMap", a_count);
        case TextureSlot::kPBREmissiveMap:  return SamplerUniform("PBREmissiveMap", a_count);
        case TextureSlot::kPBRDisplacement: return SamplerUniform("PBRDisplacementMap", a_count);
        case TextureSlot::kPBRRMAOSMap:     return SamplerUniform("PBRRMAOSMap", a_count);
        case TextureSlot::kPBRFeatures0:    return SamplerUniform("PBRFeaturesTexture0", a_count);
        case TextureSlot::kPBRFeatures1:    return SamplerUniform("PBRFeaturesTexture1", a_count);
        default:                            return NoSamplerUniforms(a_count);
    }
}

SamplerUniforms RefractionProxySamplerUniforms(const std::size_t a_slot, std::size_t& a_count) {
    return a_slot == TextureSlot::kBaseMap ? SamplerUniform("BaseMap", a_count) : NoSamplerUniforms(a_count);
}

SamplerUniforms Fo4DefaultSamplerUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:         return SamplerUniform("BaseMap", a_count);
        case TextureSlot::kNormalMap:       return SamplerUniform("NormalMap", a_count);
        case TextureSlot::kGlowMap:         return SamplerUniform("GlowMap", a_count);
        case TextureSlot::kGreyscaleMap:    return SamplerUniform("GreyscaleMap", a_count);
        case TextureSlot::kEnvironmentMap:  return SamplerUniform("CubeMap", a_count);
        case TextureSlot::kEnvironmentMask: return SamplerUniform("EnvironmentMap", a_count);
        case TextureSlot::kBacklightMap:    return MakeSamplerUniforms("BacklightMap", "SpecularMap", a_count);
        default:                            return NoSamplerUniforms(a_count);
    }
}

SamplerUniforms Fo4EffectSamplerUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kBaseMap:         return SamplerUniform("BaseMap", a_count);
        case TextureSlot::kNormalMap:       return SamplerUniform("NormalMap", a_count);
        case TextureSlot::kGreyscaleMap:    return SamplerUniform("GreyscaleMap", a_count);
        case TextureSlot::kEnvironmentMap:  return SamplerUniform("CubeMap", a_count);
        case TextureSlot::kEnvironmentMask: return SamplerUniform("SpecularMap", a_count);
        default:                            return NoSamplerUniforms(a_count);
    }
}

SamplerUniforms SamplerUniformsFor(
    const std::size_t a_slot,
    const ShaderManager::ShaderType a_shaderType,
    std::size_t& a_count
) {
    switch (a_shaderType) {
        case ShaderManager::kSKDefault:         return SkDefaultSamplerUniforms(a_slot, a_count);
        case ShaderManager::kSKMSN:             return SkMsnSamplerUniforms(a_slot, a_count);
        case ShaderManager::kSKMultilayer:      return SkMultilayerSamplerUniforms(a_slot, a_count);
        case ShaderManager::kSKEffectShader:    return SkEffectSamplerUniforms(a_slot, a_count);
        case ShaderManager::kSKPBR:             return SkPbrSamplerUniforms(a_slot, a_count);
        case ShaderManager::kSKRefractionProxy: return RefractionProxySamplerUniforms(a_slot, a_count);
        case ShaderManager::kFO4Default:        return Fo4DefaultSamplerUniforms(a_slot, a_count);
        case ShaderManager::kFO4EffectShader:   return Fo4EffectSamplerUniforms(a_slot, a_count);
        default:                                return NoSamplerUniforms(a_count);
    }
}

FeatureUniforms NoFeatureUniforms(std::size_t& a_count) {
    a_count = 0;
    return {};
}

FeatureUniforms FeatureUniform(const char* a_name, const TextureFeatureCondition a_condition, std::size_t& a_count) {
    a_count = 1;
    return {TextureFeatureUniform {.name = a_name, .condition = a_condition}, {}};
}

FeatureUniforms MaterialLoadedTextureFeatureUniform(
    const char* a_name,
    const TextureFeatureMaterialFlag a_materialFlag,
    std::size_t& a_count
) {
    a_count = 1;
    return {
        TextureFeatureUniform {
            .name = a_name,
            .condition = TextureFeatureCondition::kMaterialFlagAndLoadedTexture,
            .materialFlag = a_materialFlag,
        },
        TextureFeatureUniform(),
    };
}

FeatureUniforms MaterialAssignedTextureFeatureUniform(
    const char* a_name,
    const TextureFeatureMaterialFlag a_materialFlag,
    std::size_t& a_count
) {
    a_count = 1;
    return {
        TextureFeatureUniform {
            .name = a_name,
            .condition = TextureFeatureCondition::kMaterialFlagAndAssignedTexture,
            .materialFlag = a_materialFlag,
        },
        TextureFeatureUniform(),
    };
}

FeatureUniforms AssignedTextureFeatureUniform(const char* a_name, std::size_t& a_count) {
    return FeatureUniform(a_name, TextureFeatureCondition::kAssignedTexture, a_count);
}

FeatureUniforms LoadedTextureFeatureUniform(const char* a_name, std::size_t& a_count) {
    return FeatureUniform(a_name, TextureFeatureCondition::kLoadedTexture, a_count);
}

FeatureUniforms EnvironmentFeatureUniformsFor(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kEnvironmentMap:  return AssignedTextureFeatureUniform("hasCubeMap", a_count);
        case TextureSlot::kEnvironmentMask: return AssignedTextureFeatureUniform("hasEnvMask", a_count);
        default:                            return NoFeatureUniforms(a_count);
    }
}

FeatureUniforms SkDefaultFeatureUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kGlowMap:
            return MaterialAssignedTextureFeatureUniform("hasGlowMap", TextureFeatureMaterialFlag::kGlowMap, a_count);
        case TextureSlot::kHeightMap:
            return MaterialLoadedTextureFeatureUniform("hasHeightMap", TextureFeatureMaterialFlag::kHeightMap, a_count);
        default: return EnvironmentFeatureUniformsFor(a_slot, a_count);
    }
}

FeatureUniforms SkMsnFeatureUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kDetailMask:  return AssignedTextureFeatureUniform("hasDetailMask", a_count);
        case TextureSlot::kTintMask:    return AssignedTextureFeatureUniform("hasTintMask", a_count);
        case TextureSlot::kSpecularMap: return AssignedTextureFeatureUniform("hasSpecularMap", a_count);
        default:                        return NoFeatureUniforms(a_count);
    }
}

FeatureUniforms SkPbrFeatureUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kPBREmissiveMap:  return LoadedTextureFeatureUniform("pbrHasEmissive", a_count);
        case TextureSlot::kPBRDisplacement: return LoadedTextureFeatureUniform("pbrHasDisplacement", a_count);
        case TextureSlot::kPBRFeatures0:    return LoadedTextureFeatureUniform("pbrHasFeaturesTexture0", a_count);
        case TextureSlot::kPBRFeatures1:    return LoadedTextureFeatureUniform("pbrHasFeaturesTexture1", a_count);
        default:                            return NoFeatureUniforms(a_count);
    }
}

FeatureUniforms Fo4DefaultFeatureUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kGlowMap:
            return MaterialAssignedTextureFeatureUniform("hasGlowMap", TextureFeatureMaterialFlag::kGlowMap, a_count);
        case TextureSlot::kSpecularMap: return AssignedTextureFeatureUniform("hasSpecularMap", a_count);
        default:                        return EnvironmentFeatureUniformsFor(a_slot, a_count);
    }
}

FeatureUniforms Fo4EffectFeatureUniforms(const std::size_t a_slot, std::size_t& a_count) {
    switch (a_slot) {
        case TextureSlot::kNormalMap:       return LoadedTextureFeatureUniform("hasNormalMap", a_count);
        case TextureSlot::kEnvironmentMap:  return LoadedTextureFeatureUniform("hasCubeMap", a_count);
        case TextureSlot::kEnvironmentMask: return LoadedTextureFeatureUniform("hasEnvMask", a_count);
        default:                            return NoFeatureUniforms(a_count);
    }
}

FeatureUniforms FeatureUniformsFor(
    const std::size_t a_slot,
    const ShaderManager::ShaderType a_shaderType,
    std::size_t& a_count
) {
    switch (a_shaderType) {
        case ShaderManager::kSKDefault:       return SkDefaultFeatureUniforms(a_slot, a_count);
        case ShaderManager::kSKMultilayer:    return EnvironmentFeatureUniformsFor(a_slot, a_count);
        case ShaderManager::kSKMSN:           return SkMsnFeatureUniforms(a_slot, a_count);
        case ShaderManager::kSKPBR:           return SkPbrFeatureUniforms(a_slot, a_count);
        case ShaderManager::kFO4Default:      return Fo4DefaultFeatureUniforms(a_slot, a_count);
        case ShaderManager::kFO4EffectShader: return Fo4EffectFeatureUniforms(a_slot, a_count);
        default:                              return NoFeatureUniforms(a_count);
    }
}

ShaderManager::ShaderType SamplerShaderTypeFor(
    const ShaderManager::ShaderType a_shaderType,
    const std::size_t a_slot,
    const bool a_isRefractionProxy
) {
    if (a_isRefractionProxy && a_slot == TextureSlot::kBaseMap) {
        return ShaderManager::kSKRefractionProxy;
    }

    return a_shaderType;
}
} // namespace

TextureSlotDescriptor DescribeTextureSlot(
    const nifly::NiShader* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    const std::size_t a_slot,
    const bool a_isRefractionProxy
) {
    const auto family = FamilyForSlot(a_shader, a_shaderType, a_slot, a_isRefractionProxy);
    const auto samplerShaderType = SamplerShaderTypeFor(a_shaderType, a_slot, a_isRefractionProxy);

    std::size_t samplerUniformCount = 0;
    const auto samplerUniforms = SamplerUniformsFor(a_slot, samplerShaderType, samplerUniformCount);

    std::size_t featureUniformCount = 0;
    const auto featureUniforms = FeatureUniformsFor(a_slot, a_shaderType, featureUniformCount);

    return {
        .family = family,
        .slot = a_slot,
        .role = RoleForSlot(a_shader, a_shaderType, a_slot, family),
        .textureSetFallback = TextureSetFallback(a_shader, a_slot, family),
        .directFallback = DirectFallbackPolicy(a_shaderType, a_slot),
        .samplerUniforms = samplerUniforms,
        .samplerUniformCount = samplerUniformCount,
        .featureUniforms = featureUniforms,
        .featureUniformCount = featureUniformCount,
    };
}

TextureSlotDescriptorList TextureSlotDescriptors(
    const nifly::NiShader* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    const bool a_isRefractionProxy
) {
    TextureSlotDescriptorList descriptors {};
    for (std::size_t slot = 0; slot < descriptors.size(); ++slot) {
        descriptors[slot] = DescribeTextureSlot(a_shader, a_shaderType, slot, a_isRefractionProxy);
    }

    return descriptors;
}

QString TextureSlotDisplayName(const TextureSlotDescriptor& a_descriptor) {
    switch (a_descriptor.role) {
        case TextureSlotRole::kBase:                return QObject::tr("Base");
        case TextureSlotRole::kNormal:              return QObject::tr("Normal");
        case TextureSlotRole::kGlow:                return QObject::tr("Glow");
        case TextureSlotRole::kLight:               return QObject::tr("Light");
        case TextureSlotRole::kGreyscale:           return QObject::tr("Greyscale");
        case TextureSlotRole::kHeight:              return QObject::tr("Height");
        case TextureSlotRole::kDetail:              return QObject::tr("Detail");
        case TextureSlotRole::kEnvironment:         return QObject::tr("Environment");
        case TextureSlotRole::kEnvironmentMask:     return QObject::tr("Env Mask");
        case TextureSlotRole::kTint:                return QObject::tr("Tint");
        case TextureSlotRole::kInner:               return QObject::tr("Inner");
        case TextureSlotRole::kBacklight:           return QObject::tr("Backlight");
        case TextureSlotRole::kSpecular:            return QObject::tr("Specular");
        case TextureSlotRole::kEmissive:            return QObject::tr("Emissive");
        case TextureSlotRole::kDisplacement:        return QObject::tr("Displacement");
        case TextureSlotRole::kRMAOS:               return QObject::tr("RMAOS");
        case TextureSlotRole::kFuzzCoatNormal:      return QObject::tr("Fuzz / Coat Normal");
        case TextureSlotRole::kSubsurfaceCoatColor: return QObject::tr("Subsurface / Coat Color");
        case TextureSlotRole::kRefraction:          return QObject::tr("Refraction");
        case TextureSlotRole::kNumbered:            return QObject::tr("Slot %1").arg(a_descriptor.slot + 1);
    }

    return QObject::tr("Slot %1").arg(a_descriptor.slot + 1);
}

QString TextureSlotDisplayName(
    const nifly::NiShader* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    const std::size_t a_slot,
    const bool a_isRefractionProxy
) {
    return TextureSlotDisplayName(DescribeTextureSlot(a_shader, a_shaderType, a_slot, a_isRefractionProxy));
}
