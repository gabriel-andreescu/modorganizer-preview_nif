#pragma once

#include "Animation/ShaderValues.h"

#include <optional>

namespace nifly {
class NiTimeController;
}

namespace Animation {

// The shader value a shader property controller drives. Empty for other controllers and for values the preview
// doesn't draw.
[[nodiscard]] std::optional<ShaderFloat> ShaderFloatOf(nifly::NiTimeController* a_controller);
[[nodiscard]] std::optional<ShaderColor> ShaderColorOf(nifly::NiTimeController* a_controller);

} // namespace Animation
