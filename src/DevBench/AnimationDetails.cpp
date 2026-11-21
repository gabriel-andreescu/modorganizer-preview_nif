#include "DevBench/AnimationDetails.h"
#include "Animation/ShaderValues.h"
#include "Nif/SceneGraph.h"

#include <QJsonArray>

#include <array>

namespace {
constexpr std::array kFloatNames {
    "emissiveMultiple",
    "alpha",
    "uOffset",
    "uScale",
    "vOffset",
    "vScale",
    "falloffStartAngle",
    "falloffStopAngle",
    "falloffStartOpacity",
    "falloffStopOpacity",
    "refractionStrength",
    "reflectionStrength",
    "glossiness",
    "specularStrength",
};
static_assert(kFloatNames.size() == std::to_underlying(Animation::ShaderFloat::kCount));

constexpr std::array kColorNames {"emissiveColor", "specularColor"};
static_assert(kColorNames.size() == std::to_underlying(Animation::ShaderColor::kCount));
} // namespace

namespace DevBench {

QJsonObject ShaderValuesDetails(const Animation::ShaderValues& a_values) {
    QJsonObject details;
    for (std::size_t i = 0; i < kFloatNames.size(); ++i) {
        if (const auto& value = a_values.floats[i]) {
            details.insert(QLatin1StringView(kFloatNames[i]), *value);
        }
    }
    for (std::size_t i = 0; i < kColorNames.size(); ++i) {
        if (const auto& color = a_values.colors[i]) {
            details.insert(QLatin1StringView(kColorNames[i]), QJsonArray {color->x, color->y, color->z});
        }
    }
    return details;
}

QJsonObject ObjectStateDetails(const ObjectState& a_state) {
    const auto& world = a_state.world;
    QJsonArray rotation;
    for (auto i = 0; i < 3; ++i) {
        const auto& row = world.rotation[i];
        rotation.append(QJsonArray {row.x, row.y, row.z});
    }
    return {
        {"translation", QJsonArray {world.translation.x, world.translation.y, world.translation.z}},
        {"rotation", rotation},
        {"scale", world.scale},
        {"hidden", a_state.hidden},
    };
}

}
