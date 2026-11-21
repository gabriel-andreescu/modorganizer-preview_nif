#pragma once

#include "Render/ShaderManager.h"

namespace nifly {
class NifFile;
class NiShader;
}

[[nodiscard]] ShaderManager::ShaderType ClassifyShaderType(
    const nifly::NifFile* a_nifFile,
    const nifly::NiShader* a_shader
);
[[nodiscard]] bool IsSkyrimLightingShader(ShaderManager::ShaderType a_shaderType);
