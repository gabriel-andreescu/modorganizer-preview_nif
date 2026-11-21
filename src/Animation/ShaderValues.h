#pragma once

#include <nifly/Object3d.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace Animation {

// The shader values a controller can animate, whichever shader type holds them.
enum class ShaderFloat : std::uint8_t {
    kEmissiveMultiple,
    kAlpha,
    kUOffset,
    kUScale,
    kVOffset,
    kVScale,
    kFalloffStartAngle,
    kFalloffStopAngle,
    kFalloffStartOpacity,
    kFalloffStopOpacity,
    kRefractionStrength,
    kReflectionStrength,
    kGlossiness,
    kSpecularStrength,
    kCount,
};

enum class ShaderColor : std::uint8_t {
    kEmissive,
    kSpecular,
    kCount,
};

// One shader's animated values at a moment. A value no controller drives is empty.
struct ShaderValues {
    std::array<std::optional<float>, std::to_underlying(ShaderFloat::kCount)> floats;
    std::array<std::optional<nifly::Vector3>, std::to_underlying(ShaderColor::kCount)> colors;

    [[nodiscard]] const std::optional<float>& operator[](const ShaderFloat a_value) const {
        return floats[std::to_underlying(a_value)];
    }
    [[nodiscard]] std::optional<float>& operator[](const ShaderFloat a_value) {
        return floats[std::to_underlying(a_value)];
    }
    [[nodiscard]] const std::optional<nifly::Vector3>& operator[](const ShaderColor a_value) const {
        return colors[std::to_underlying(a_value)];
    }
    [[nodiscard]] std::optional<nifly::Vector3>& operator[](const ShaderColor a_value) {
        return colors[std::to_underlying(a_value)];
    }
};

} // namespace Animation
