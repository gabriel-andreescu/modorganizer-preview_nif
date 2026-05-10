#pragma once

#include <cstddef>

enum TextureSlot {
    BaseMap = 0,
    NormalMap = 1,
    GlowMap = 2,
    LightMask = 2,
    PBREmissiveMap = 2,
    GreyscaleMap = 3,
    HeightMap = 3,
    DetailMask = 3,
    PBRDisplacement = 3,
    EnvironmentMap = 4,
    EnvironmentMask = 5,
    PBRRMAOSMap = 5,
    TintMask = 6,
    InnerMap = 6,
    PBRFeatures1 = 6,
    BacklightMap = 7,
    SpecularMap = 7,
    PBRFeatures0 = 7,
};

constexpr std::size_t TextureSlotCount = 13;
