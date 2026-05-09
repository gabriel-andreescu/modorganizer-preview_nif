#pragma once

#include <cstddef>

enum TextureSlot
{
  BaseMap         = 0,
  NormalMap       = 1,
  GlowMap         = 2,
  LightMask       = 2,
  HeightMap       = 3,
  DetailMask      = 3,
  EnvironmentMap  = 4,
  EnvironmentMask = 5,
  TintMask        = 6,
  InnerMap        = 6,
  BacklightMap    = 7,
  SpecularMap     = 7,
};

constexpr std::size_t TextureSlotCount = 13;
