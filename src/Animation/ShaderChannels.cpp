#include "Animation/ShaderChannels.h"

#include <nifly/Animation.hpp>

namespace {
using Animation::ShaderColor;
using Animation::ShaderFloat;

// The controlled variable numbers of BSEffectShaderPropertyFloatController.
std::optional<ShaderFloat> EffectFloat(const std::uint32_t a_variable) {
    switch (a_variable) {
        case 0:  return ShaderFloat::kEmissiveMultiple;
        case 1:  return ShaderFloat::kFalloffStartAngle;
        case 2:  return ShaderFloat::kFalloffStopAngle;
        case 3:  return ShaderFloat::kFalloffStartOpacity;
        case 4:  return ShaderFloat::kFalloffStopOpacity;
        case 5:  return ShaderFloat::kAlpha;
        case 6:  return ShaderFloat::kUOffset;
        case 7:  return ShaderFloat::kUScale;
        case 8:  return ShaderFloat::kVOffset;
        case 9:  return ShaderFloat::kVScale;
        default: return std::nullopt;
    }
}

// The controlled variable numbers of BSLightingShaderPropertyFloatController.
std::optional<ShaderFloat> LightingFloat(const std::uint32_t a_variable) {
    switch (a_variable) {
        case 0:  return ShaderFloat::kRefractionStrength;
        case 8:  return ShaderFloat::kReflectionStrength;
        case 9:  return ShaderFloat::kGlossiness;
        case 10: return ShaderFloat::kSpecularStrength;
        case 11: return ShaderFloat::kEmissiveMultiple;
        case 12: return ShaderFloat::kAlpha;
        case 20: return ShaderFloat::kUOffset;
        case 21: return ShaderFloat::kUScale;
        case 22: return ShaderFloat::kVOffset;
        case 23: return ShaderFloat::kVScale;
        default: return std::nullopt;
    }
}
} // namespace

namespace Animation {

std::optional<ShaderFloat> ShaderFloatOf(nifly::NiTimeController* const a_controller) {
    if (const auto* const effect = dynamic_cast<nifly::BSEffectShaderPropertyFloatController*>(a_controller)) {
        return EffectFloat(effect->typeOfControlledVariable);
    }
    if (const auto* const lighting = dynamic_cast<nifly::BSLightingShaderPropertyFloatController*>(a_controller)) {
        return LightingFloat(lighting->typeOfControlledVariable);
    }
    return std::nullopt;
}

std::optional<ShaderColor> ShaderColorOf(nifly::NiTimeController* const a_controller) {
    // An effect shader's only color is its base color, which the shader blocks store as the emissive color.
    if (const auto* const effect = dynamic_cast<nifly::BSEffectShaderPropertyColorController*>(a_controller)) {
        return effect->typeOfControlledColor == 0 ? std::optional(ShaderColor::kEmissive) : std::nullopt;
    }
    if (const auto* const lighting = dynamic_cast<nifly::BSLightingShaderPropertyColorController*>(a_controller)) {
        switch (lighting->typeOfControlledColor) {
            case 0:  return ShaderColor::kSpecular;
            case 1:  return ShaderColor::kEmissive;
            default: return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace Animation
