#pragma once

#include "Render/ShaderManager.h"
#include "Textures/Slots.h"

#include <QString>

#include <array>
#include <cstddef>

namespace nifly {
class NiShader;
}

enum class TextureSlotFamily {
    kStandard,
    kEffect,
    kPBR,
    kRefractionProxy,
};

enum class TextureSlotRole {
    kBase,
    kNormal,
    kGlow,
    kLight,
    kGreyscale,
    kHeight,
    kDetail,
    kEnvironment,
    kEnvironmentMask,
    kTint,
    kInner,
    kBacklight,
    kSpecular,
    kEmissive,
    kDisplacement,
    kRMAOS,
    kFuzzCoatNormal,
    kSubsurfaceCoatColor,
    kRefraction,
    kNumbered,
};

enum class TextureFallback {
    kNone,
    kError,
    kFlatNormal,
    kBlack,
    kWhite,
};

struct TextureSamplerUniform {
    const char* name = nullptr;
};

enum class TextureFeatureCondition {
    kAssignedTexture,
    kLoadedTexture,
    kMaterialFlagAndAssignedTexture,
    kMaterialFlagAndLoadedTexture,
};

enum class TextureFeatureMaterialFlag {
    kNone,
    kGlowMap,
    kHeightMap,
};

struct TextureFeatureUniform {
    const char* name = nullptr;
    TextureFeatureCondition condition = TextureFeatureCondition::kAssignedTexture;
    TextureFeatureMaterialFlag materialFlag = TextureFeatureMaterialFlag::kNone;
};

struct TextureFallbackPolicy {
    TextureFallback emptyPath = TextureFallback::kNone;
    TextureFallback failedLoad = TextureFallback::kNone;
};

struct TextureSlotDescriptor {
    TextureSlotFamily family = TextureSlotFamily::kStandard;
    std::size_t slot = 0;
    TextureSlotRole role = TextureSlotRole::kNumbered;
    TextureFallback textureSetFallback = TextureFallback::kNone;
    TextureFallbackPolicy directFallback {};
    std::array<TextureSamplerUniform, 2> samplerUniforms {};
    std::size_t samplerUniformCount = 0;
    std::array<TextureFeatureUniform, 2> featureUniforms {};
    std::size_t featureUniformCount = 0;
};

using TextureSlotDescriptorList = std::array<TextureSlotDescriptor, kTextureSlotCount>;

[[nodiscard]] TextureSlotDescriptor DescribeTextureSlot(
    const nifly::NiShader* a_shader,
    ShaderManager::ShaderType a_shaderType,
    std::size_t a_slot,
    bool a_isRefractionProxy = false
);
[[nodiscard]] TextureSlotDescriptorList TextureSlotDescriptors(
    const nifly::NiShader* a_shader,
    ShaderManager::ShaderType a_shaderType,
    bool a_isRefractionProxy = false
);
[[nodiscard]] QString TextureSlotDisplayName(const TextureSlotDescriptor& a_descriptor);
[[nodiscard]] QString TextureSlotDisplayName(
    const nifly::NiShader* a_shader,
    ShaderManager::ShaderType a_shaderType,
    std::size_t a_slot,
    bool a_isRefractionProxy = false
);
