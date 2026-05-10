#pragma once

#include "ShaderManager.h"
#include "TextureSlots.h"

#include <QString>

#include <array>
#include <cstddef>

namespace nifly {
class NiShader;
}

enum class TextureSlotFamily {
    Standard,
    Effect,
    PBR,
    RefractionProxy
};

enum class TextureSlotRole {
    Base,
    Normal,
    Glow,
    Light,
    Greyscale,
    Height,
    Detail,
    Environment,
    EnvironmentMask,
    Tint,
    Inner,
    Backlight,
    Specular,
    Emissive,
    Displacement,
    RMAOS,
    FuzzCoatNormal,
    SubsurfaceCoatColor,
    Refraction,
    Numbered
};

enum class TextureFallback {
    None,
    Error,
    FlatNormal,
    Black,
    White
};

struct TextureSamplerUniform {
    const char* name = nullptr;
};

enum class TextureFeatureCondition {
    AssignedTexture,
    LoadedTexture,
    MaterialFlagAndAssignedTexture,
    MaterialFlagAndLoadedTexture
};

enum class TextureFeatureMaterialFlag {
    None,
    GlowMap,
    HeightMap
};

struct TextureFeatureUniform {
    const char* name = nullptr;
    TextureFeatureCondition condition = TextureFeatureCondition::AssignedTexture;
    TextureFeatureMaterialFlag materialFlag = TextureFeatureMaterialFlag::None;
};

struct TextureFallbackPolicy {
    TextureFallback emptyPath = TextureFallback::None;
    TextureFallback failedLoad = TextureFallback::None;
};

struct TextureSlotDescriptor {
    TextureSlotFamily family = TextureSlotFamily::Standard;
    std::size_t slot = 0;
    TextureSlotRole role = TextureSlotRole::Numbered;
    TextureFallback textureSetFallback = TextureFallback::None;
    TextureFallbackPolicy directFallback {};
    std::array<TextureSamplerUniform, 2> samplerUniforms {};
    std::size_t samplerUniformCount = 0;
    std::array<TextureFeatureUniform, 2> featureUniforms {};
    std::size_t featureUniformCount = 0;
};

using TextureSlotDescriptorList = std::array<TextureSlotDescriptor, TextureSlotCount>;

[[nodiscard]] TextureSlotDescriptor textureSlotDescriptor(
    const nifly::NiShader* shader,
    ShaderManager::ShaderType shaderType,
    std::size_t slot,
    bool isRefractionProxy = false
);
[[nodiscard]] TextureSlotDescriptorList textureSlotDescriptors(
    const nifly::NiShader* shader,
    ShaderManager::ShaderType shaderType,
    bool isRefractionProxy = false
);
[[nodiscard]] QString textureSlotDisplayName(const TextureSlotDescriptor& descriptor);
[[nodiscard]] QString textureSlotDisplayName(
    const nifly::NiShader* shader,
    ShaderManager::ShaderType shaderType,
    std::size_t slot,
    bool isRefractionProxy = false
);
