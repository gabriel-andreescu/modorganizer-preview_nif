#pragma once

#include <cstddef>
#include <cstdint>

enum TextureSlot : std::uint8_t {
    kBaseMap = 0,
    kNormalMap = 1,
    kGlowMap = 2,
    kLightMask = 2,
    kPBREmissiveMap = 2,
    kGreyscaleMap = 3,
    kHeightMap = 3,
    kDetailMask = 3,
    kPBRDisplacement = 3,
    kEnvironmentMap = 4,
    kEnvironmentMask = 5,
    kPBRRMAOSMap = 5,
    kTintMask = 6,
    kInnerMap = 6,
    kPBRFeatures1 = 6,
    kBacklightMap = 7,
    kSpecularMap = 7,
    kPBRFeatures0 = 7,
};

constexpr std::size_t kTextureSlotCount = 13;
