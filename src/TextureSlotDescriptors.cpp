#include "TextureSlotDescriptors.h"
#include "NifShaderUtils.h"

#include <QObject>

namespace {
using SamplerUniforms = std::array<TextureSamplerUniform, 2>;
using FeatureUniforms = std::array<TextureFeatureUniform, 2>;

bool isEffectShaderType(const ShaderManager::ShaderType shaderType) {
    return shaderType == ShaderManager::SKEffectShader || shaderType == ShaderManager::FO4EffectShader;
}

bool isMultilayerShader(const nifly::NiShader* shader) {
    const auto* const bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(shader);
    return bslsp && bslsp->GetShaderType() == nifly::BSLSP_MULTILAYERPARALLAX;
}

bool usesHeightSlot(const nifly::NiShader* shader) {
    const auto* const bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(shader);
    if (!bslsp) {
        return false;
    }

    const auto shaderType = bslsp->GetShaderType();
    return shaderType == nifly::BSLSP_PARALLAX || shaderType == nifly::BSLSP_PARALLAXOCC;
}

TextureSlotFamily textureSlotFamily(
    const nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const std::size_t slot,
    const bool isRefractionProxy
) {
    if ((isRefractionProxy || shaderType == ShaderManager::SKRefractionProxy) && slot == TextureSlot::BaseMap) {
        return TextureSlotFamily::RefractionProxy;
    }
    if (shaderType == ShaderManager::SKPBR) {
        return TextureSlotFamily::PBR;
    }
    if (isEffectShaderType(shaderType)) {
        return TextureSlotFamily::Effect;
    }
    if (shaderType == ShaderManager::None) {
        if (IsPBRLightingShader(shader)) {
            return TextureSlotFamily::PBR;
        }
        if (dynamic_cast<const nifly::BSEffectShaderProperty*>(shader)) {
            return TextureSlotFamily::Effect;
        }
    }

    return TextureSlotFamily::Standard;
}

TextureSlotRole standardTextureSlotRole(
    const nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const std::size_t slot
) {
    switch (slot) {
        case TextureSlot::BaseMap:   return TextureSlotRole::Base;
        case TextureSlot::NormalMap: return TextureSlotRole::Normal;
        case TextureSlot::GlowMap:
            return shader && shader->HasGlowmap() ? TextureSlotRole::Glow : TextureSlotRole::Light;
        case TextureSlot::HeightMap:
            if (shaderType == ShaderManager::FO4Default) {
                return TextureSlotRole::Greyscale;
            }
            if (usesHeightSlot(shader)) {
                return TextureSlotRole::Height;
            }
            if (shaderType == ShaderManager::SKMultilayer) {
                return TextureSlotRole::Numbered;
            }
            return TextureSlotRole::Detail;
        case TextureSlot::EnvironmentMap:  return TextureSlotRole::Environment;
        case TextureSlot::EnvironmentMask: return TextureSlotRole::EnvironmentMask;
        case TextureSlot::TintMask:        return isMultilayerShader(shader) ? TextureSlotRole::Inner : TextureSlotRole::Tint;
        case TextureSlot::BacklightMap:
            return shader && shader->HasBacklight() ? TextureSlotRole::Backlight : TextureSlotRole::Specular;
        default: return TextureSlotRole::Numbered;
    }
}

TextureSlotRole effectTextureSlotRole(const ShaderManager::ShaderType shaderType, const std::size_t slot) {
    switch (slot) {
        case TextureSlot::BaseMap:      return TextureSlotRole::Base;
        case TextureSlot::GreyscaleMap: return TextureSlotRole::Greyscale;
        case TextureSlot::NormalMap:
            return shaderType == ShaderManager::FO4EffectShader ? TextureSlotRole::Normal : TextureSlotRole::Numbered;
        case TextureSlot::EnvironmentMap:
            return shaderType == ShaderManager::FO4EffectShader ? TextureSlotRole::Environment
                                                                : TextureSlotRole::Numbered;
        case TextureSlot::EnvironmentMask:
            return shaderType == ShaderManager::FO4EffectShader ? TextureSlotRole::EnvironmentMask
                                                                : TextureSlotRole::Numbered;
        default: return TextureSlotRole::Numbered;
    }
}

TextureSlotRole pbrTextureSlotRole(const std::size_t slot) {
    switch (slot) {
        case TextureSlot::BaseMap:         return TextureSlotRole::Base;
        case TextureSlot::NormalMap:       return TextureSlotRole::Normal;
        case TextureSlot::PBREmissiveMap:  return TextureSlotRole::Emissive;
        case TextureSlot::PBRDisplacement: return TextureSlotRole::Displacement;
        case TextureSlot::PBRRMAOSMap:     return TextureSlotRole::RMAOS;
        case TextureSlot::PBRFeatures1:    return TextureSlotRole::FuzzCoatNormal;
        case TextureSlot::PBRFeatures0:    return TextureSlotRole::SubsurfaceCoatColor;
        default:                           return TextureSlotRole::Numbered;
    }
}

TextureSlotRole textureSlotRole(
    const nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const std::size_t slot,
    const TextureSlotFamily family
) {
    switch (family) {
        case TextureSlotFamily::RefractionProxy: return TextureSlotRole::Refraction;
        case TextureSlotFamily::PBR:             return pbrTextureSlotRole(slot);
        case TextureSlotFamily::Effect:          return effectTextureSlotRole(shaderType, slot);
        case TextureSlotFamily::Standard:        return standardTextureSlotRole(shader, shaderType, slot);
    }

    return TextureSlotRole::Numbered;
}

TextureFallback pbrTextureSetFallback(const std::size_t slot) {
    switch (slot) {
        case TextureSlot::BaseMap:         return TextureFallback::Error;
        case TextureSlot::NormalMap:       return TextureFallback::FlatNormal;
        case TextureSlot::PBREmissiveMap:
        case TextureSlot::PBRDisplacement: return TextureFallback::Black;
        case TextureSlot::PBRRMAOSMap:
        case TextureSlot::PBRFeatures0:
        case TextureSlot::PBRFeatures1:    return TextureFallback::White;
        default:                           return TextureFallback::None;
    }
}

TextureFallback standardTextureSetFallback(const nifly::NiShader* shader, const std::size_t slot) {
    switch (slot) {
        case TextureSlot::BaseMap:   return TextureFallback::Error;
        case TextureSlot::NormalMap: return TextureFallback::FlatNormal;
        case TextureSlot::GlowMap:
            return shader && shader->HasGlowmap() ? TextureFallback::Black : TextureFallback::White;
        default: return TextureFallback::None;
    }
}

TextureFallback textureSetFallback(
    const nifly::NiShader* shader,
    const std::size_t slot,
    const TextureSlotFamily family
) {
    switch (family) {
        case TextureSlotFamily::PBR: return pbrTextureSetFallback(slot);
        case TextureSlotFamily::RefractionProxy:
            return slot == TextureSlot::BaseMap ? TextureFallback::Error : TextureFallback::None;
        case TextureSlotFamily::Effect:   return TextureFallback::None;
        case TextureSlotFamily::Standard: return standardTextureSetFallback(shader, slot);
    }

    return TextureFallback::None;
}

TextureFallbackPolicy fallbackPolicy(const TextureFallback emptyPath, const TextureFallback failedLoad) {
    return {.emptyPath = emptyPath, .failedLoad = failedLoad};
}

TextureFallbackPolicy effectDirectFallbackPolicy(const ShaderManager::ShaderType shaderType, const std::size_t slot) {
    switch (slot) {
        case TextureSlot::BaseMap:
        case TextureSlot::GreyscaleMap: return fallbackPolicy(TextureFallback::White, TextureFallback::Error);
        case TextureSlot::NormalMap:
            return shaderType == ShaderManager::FO4EffectShader
                       ? fallbackPolicy(TextureFallback::FlatNormal, TextureFallback::FlatNormal)
                       : TextureFallbackPolicy {};
        case TextureSlot::EnvironmentMap:
            return shaderType == ShaderManager::FO4EffectShader
                       ? fallbackPolicy(TextureFallback::None, TextureFallback::None)
                       : TextureFallbackPolicy {};
        case TextureSlot::EnvironmentMask:
            return shaderType == ShaderManager::FO4EffectShader
                       ? fallbackPolicy(TextureFallback::White, TextureFallback::Error)
                       : TextureFallbackPolicy {};
        default: return {};
    }
}

TextureFallbackPolicy directFallbackPolicy(const ShaderManager::ShaderType shaderType, const std::size_t slot) {
    if (isEffectShaderType(shaderType)) {
        return effectDirectFallbackPolicy(shaderType, slot);
    }

    return {};
}

SamplerUniforms noSamplerUniforms(std::size_t& count) {
    count = 0;
    return {};
}

SamplerUniforms samplerUniform(const char* name, std::size_t& count) {
    count = 1;
    return {TextureSamplerUniform {.name = name}, {}};
}

SamplerUniforms samplerUniforms(const char* first, const char* second, std::size_t& count) {
    count = 2;
    return {TextureSamplerUniform {.name = first}, TextureSamplerUniform {.name = second}};
}

SamplerUniforms skDefaultSamplerUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::BaseMap:         return samplerUniform("BaseMap", count);
        case TextureSlot::NormalMap:       return samplerUniform("NormalMap", count);
        case TextureSlot::GlowMap:         return samplerUniforms("GlowMap", "LightMask", count);
        case TextureSlot::HeightMap:       return samplerUniform("HeightMap", count);
        case TextureSlot::EnvironmentMap:  return samplerUniform("CubeMap", count);
        case TextureSlot::EnvironmentMask: return samplerUniform("EnvironmentMap", count);
        case TextureSlot::BacklightMap:    return samplerUniform("BacklightMap", count);
        default:                           return noSamplerUniforms(count);
    }
}

SamplerUniforms skMsnSamplerUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::BaseMap:      return samplerUniform("BaseMap", count);
        case TextureSlot::NormalMap:    return samplerUniform("NormalMap", count);
        case TextureSlot::LightMask:    return samplerUniform("LightMask", count);
        case TextureSlot::DetailMask:   return samplerUniform("DetailMask", count);
        case TextureSlot::TintMask:     return samplerUniform("TintMask", count);
        case TextureSlot::BacklightMap: return samplerUniforms("BacklightMap", "SpecularMap", count);
        default:                        return noSamplerUniforms(count);
    }
}

SamplerUniforms skMultilayerSamplerUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::BaseMap:         return samplerUniform("BaseMap", count);
        case TextureSlot::NormalMap:       return samplerUniform("NormalMap", count);
        case TextureSlot::LightMask:       return samplerUniform("LightMask", count);
        case TextureSlot::EnvironmentMap:  return samplerUniform("CubeMap", count);
        case TextureSlot::EnvironmentMask: return samplerUniform("EnvironmentMap", count);
        case TextureSlot::InnerMap:        return samplerUniform("InnerMap", count);
        case TextureSlot::BacklightMap:    return samplerUniform("BacklightMap", count);
        default:                           return noSamplerUniforms(count);
    }
}

SamplerUniforms skEffectSamplerUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::BaseMap:      return samplerUniform("BaseMap", count);
        case TextureSlot::GreyscaleMap: return samplerUniform("GreyscaleMap", count);
        default:                        return noSamplerUniforms(count);
    }
}

SamplerUniforms skPbrSamplerUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::BaseMap:         return samplerUniform("BaseMap", count);
        case TextureSlot::NormalMap:       return samplerUniform("NormalMap", count);
        case TextureSlot::PBREmissiveMap:  return samplerUniform("PBREmissiveMap", count);
        case TextureSlot::PBRDisplacement: return samplerUniform("PBRDisplacementMap", count);
        case TextureSlot::PBRRMAOSMap:     return samplerUniform("PBRRMAOSMap", count);
        case TextureSlot::PBRFeatures0:    return samplerUniform("PBRFeaturesTexture0", count);
        case TextureSlot::PBRFeatures1:    return samplerUniform("PBRFeaturesTexture1", count);
        default:                           return noSamplerUniforms(count);
    }
}

SamplerUniforms refractionProxySamplerUniforms(const std::size_t slot, std::size_t& count) {
    return slot == TextureSlot::BaseMap ? samplerUniform("BaseMap", count) : noSamplerUniforms(count);
}

SamplerUniforms fo4DefaultSamplerUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::BaseMap:         return samplerUniform("BaseMap", count);
        case TextureSlot::NormalMap:       return samplerUniform("NormalMap", count);
        case TextureSlot::GlowMap:         return samplerUniform("GlowMap", count);
        case TextureSlot::GreyscaleMap:    return samplerUniform("GreyscaleMap", count);
        case TextureSlot::EnvironmentMap:  return samplerUniform("CubeMap", count);
        case TextureSlot::EnvironmentMask: return samplerUniform("EnvironmentMap", count);
        case TextureSlot::BacklightMap:    return samplerUniforms("BacklightMap", "SpecularMap", count);
        default:                           return noSamplerUniforms(count);
    }
}

SamplerUniforms fo4EffectSamplerUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::BaseMap:         return samplerUniform("BaseMap", count);
        case TextureSlot::NormalMap:       return samplerUniform("NormalMap", count);
        case TextureSlot::GreyscaleMap:    return samplerUniform("GreyscaleMap", count);
        case TextureSlot::EnvironmentMap:  return samplerUniform("CubeMap", count);
        case TextureSlot::EnvironmentMask: return samplerUniform("SpecularMap", count);
        default:                           return noSamplerUniforms(count);
    }
}

SamplerUniforms samplerUniformsFor(
    const std::size_t slot,
    const ShaderManager::ShaderType shaderType,
    std::size_t& count
) {
    switch (shaderType) {
        case ShaderManager::SKDefault:         return skDefaultSamplerUniforms(slot, count);
        case ShaderManager::SKMSN:             return skMsnSamplerUniforms(slot, count);
        case ShaderManager::SKMultilayer:      return skMultilayerSamplerUniforms(slot, count);
        case ShaderManager::SKEffectShader:    return skEffectSamplerUniforms(slot, count);
        case ShaderManager::SKPBR:             return skPbrSamplerUniforms(slot, count);
        case ShaderManager::SKRefractionProxy: return refractionProxySamplerUniforms(slot, count);
        case ShaderManager::FO4Default:        return fo4DefaultSamplerUniforms(slot, count);
        case ShaderManager::FO4EffectShader:   return fo4EffectSamplerUniforms(slot, count);
        default:                               return noSamplerUniforms(count);
    }
}

FeatureUniforms noFeatureUniforms(std::size_t& count) {
    count = 0;
    return {};
}

FeatureUniforms featureUniform(const char* name, const TextureFeatureCondition condition, std::size_t& count) {
    count = 1;
    return {TextureFeatureUniform {.name = name, .condition = condition}, {}};
}

FeatureUniforms materialLoadedTextureFeatureUniform(
    const char* name,
    const TextureFeatureMaterialFlag materialFlag,
    std::size_t& count
) {
    count = 1;
    return {
        TextureFeatureUniform {
            .name = name,
            .condition = TextureFeatureCondition::MaterialFlagAndLoadedTexture,
            .materialFlag = materialFlag,
        },
        {},
    };
}

FeatureUniforms materialAssignedTextureFeatureUniform(
    const char* name,
    const TextureFeatureMaterialFlag materialFlag,
    std::size_t& count
) {
    count = 1;
    return {
        TextureFeatureUniform {
            .name = name,
            .condition = TextureFeatureCondition::MaterialFlagAndAssignedTexture,
            .materialFlag = materialFlag,
        },
        {},
    };
}

FeatureUniforms assignedTextureFeatureUniform(const char* name, std::size_t& count) {
    return featureUniform(name, TextureFeatureCondition::AssignedTexture, count);
}

FeatureUniforms loadedTextureFeatureUniform(const char* name, std::size_t& count) {
    return featureUniform(name, TextureFeatureCondition::LoadedTexture, count);
}

FeatureUniforms environmentFeatureUniformsFor(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::EnvironmentMap:  return assignedTextureFeatureUniform("hasCubeMap", count);
        case TextureSlot::EnvironmentMask: return assignedTextureFeatureUniform("hasEnvMask", count);
        default:                           return noFeatureUniforms(count);
    }
}

FeatureUniforms skDefaultFeatureUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::GlowMap:
            return materialAssignedTextureFeatureUniform("hasGlowMap", TextureFeatureMaterialFlag::GlowMap, count);
        case TextureSlot::HeightMap:
            return materialLoadedTextureFeatureUniform("hasHeightMap", TextureFeatureMaterialFlag::HeightMap, count);
        default: return environmentFeatureUniformsFor(slot, count);
    }
}

FeatureUniforms skMsnFeatureUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::DetailMask:  return assignedTextureFeatureUniform("hasDetailMask", count);
        case TextureSlot::TintMask:    return assignedTextureFeatureUniform("hasTintMask", count);
        case TextureSlot::SpecularMap: return assignedTextureFeatureUniform("hasSpecularMap", count);
        default:                       return noFeatureUniforms(count);
    }
}

FeatureUniforms skPbrFeatureUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::PBREmissiveMap:  return loadedTextureFeatureUniform("pbrHasEmissive", count);
        case TextureSlot::PBRDisplacement: return loadedTextureFeatureUniform("pbrHasDisplacement", count);
        case TextureSlot::PBRFeatures0:    return loadedTextureFeatureUniform("pbrHasFeaturesTexture0", count);
        case TextureSlot::PBRFeatures1:    return loadedTextureFeatureUniform("pbrHasFeaturesTexture1", count);
        default:                           return noFeatureUniforms(count);
    }
}

FeatureUniforms fo4DefaultFeatureUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::GlowMap:
            return materialAssignedTextureFeatureUniform("hasGlowMap", TextureFeatureMaterialFlag::GlowMap, count);
        case TextureSlot::SpecularMap: return assignedTextureFeatureUniform("hasSpecularMap", count);
        default:                       return environmentFeatureUniformsFor(slot, count);
    }
}

FeatureUniforms fo4EffectFeatureUniforms(const std::size_t slot, std::size_t& count) {
    switch (slot) {
        case TextureSlot::NormalMap:       return loadedTextureFeatureUniform("hasNormalMap", count);
        case TextureSlot::EnvironmentMap:  return loadedTextureFeatureUniform("hasCubeMap", count);
        case TextureSlot::EnvironmentMask: return loadedTextureFeatureUniform("hasEnvMask", count);
        default:                           return noFeatureUniforms(count);
    }
}

FeatureUniforms featureUniformsFor(
    const std::size_t slot,
    const ShaderManager::ShaderType shaderType,
    std::size_t& count
) {
    switch (shaderType) {
        case ShaderManager::SKDefault:       return skDefaultFeatureUniforms(slot, count);
        case ShaderManager::SKMultilayer:    return environmentFeatureUniformsFor(slot, count);
        case ShaderManager::SKMSN:           return skMsnFeatureUniforms(slot, count);
        case ShaderManager::SKPBR:           return skPbrFeatureUniforms(slot, count);
        case ShaderManager::FO4Default:      return fo4DefaultFeatureUniforms(slot, count);
        case ShaderManager::FO4EffectShader: return fo4EffectFeatureUniforms(slot, count);
        default:                             return noFeatureUniforms(count);
    }
}

ShaderManager::ShaderType samplerShaderTypeFor(
    const ShaderManager::ShaderType shaderType,
    const std::size_t slot,
    const bool isRefractionProxy
) {
    if (isRefractionProxy && slot == TextureSlot::BaseMap) {
        return ShaderManager::SKRefractionProxy;
    }

    return shaderType;
}
} // namespace

TextureSlotDescriptor textureSlotDescriptor(
    const nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const std::size_t slot,
    const bool isRefractionProxy
) {
    const auto family = textureSlotFamily(shader, shaderType, slot, isRefractionProxy);
    const auto samplerShaderType = samplerShaderTypeFor(shaderType, slot, isRefractionProxy);

    std::size_t samplerUniformCount = 0;
    auto samplerUniforms = samplerUniformsFor(slot, samplerShaderType, samplerUniformCount);

    std::size_t featureUniformCount = 0;
    auto featureUniforms = featureUniformsFor(slot, shaderType, featureUniformCount);

    return {
        .family = family,
        .slot = slot,
        .role = textureSlotRole(shader, shaderType, slot, family),
        .textureSetFallback = textureSetFallback(shader, slot, family),
        .directFallback = directFallbackPolicy(shaderType, slot),
        .samplerUniforms = samplerUniforms,
        .samplerUniformCount = samplerUniformCount,
        .featureUniforms = featureUniforms,
        .featureUniformCount = featureUniformCount,
    };
}

TextureSlotDescriptorList textureSlotDescriptors(
    const nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const bool isRefractionProxy
) {
    TextureSlotDescriptorList descriptors {};
    for (std::size_t slot = 0; slot < descriptors.size(); ++slot) {
        descriptors[slot] = textureSlotDescriptor(shader, shaderType, slot, isRefractionProxy);
    }

    return descriptors;
}

QString textureSlotDisplayName(const TextureSlotDescriptor& descriptor) {
    switch (descriptor.role) {
        case TextureSlotRole::Base:                return QObject::tr("Base");
        case TextureSlotRole::Normal:              return QObject::tr("Normal");
        case TextureSlotRole::Glow:                return QObject::tr("Glow");
        case TextureSlotRole::Light:               return QObject::tr("Light");
        case TextureSlotRole::Greyscale:           return QObject::tr("Greyscale");
        case TextureSlotRole::Height:              return QObject::tr("Height");
        case TextureSlotRole::Detail:              return QObject::tr("Detail");
        case TextureSlotRole::Environment:         return QObject::tr("Environment");
        case TextureSlotRole::EnvironmentMask:     return QObject::tr("Env Mask");
        case TextureSlotRole::Tint:                return QObject::tr("Tint");
        case TextureSlotRole::Inner:               return QObject::tr("Inner");
        case TextureSlotRole::Backlight:           return QObject::tr("Backlight");
        case TextureSlotRole::Specular:            return QObject::tr("Specular");
        case TextureSlotRole::Emissive:            return QObject::tr("Emissive");
        case TextureSlotRole::Displacement:        return QObject::tr("Displacement");
        case TextureSlotRole::RMAOS:               return QObject::tr("RMAOS");
        case TextureSlotRole::FuzzCoatNormal:      return QObject::tr("Fuzz / Coat Normal");
        case TextureSlotRole::SubsurfaceCoatColor: return QObject::tr("Subsurface / Coat Color");
        case TextureSlotRole::Refraction:          return QObject::tr("Refraction");
        case TextureSlotRole::Numbered:            return QObject::tr("Slot %1").arg(descriptor.slot + 1);
    }

    return QObject::tr("Slot %1").arg(descriptor.slot + 1);
}

QString textureSlotDisplayName(
    const nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const std::size_t slot,
    const bool isRefractionProxy
) {
    return textureSlotDisplayName(textureSlotDescriptor(shader, shaderType, slot, isRefractionProxy));
}
