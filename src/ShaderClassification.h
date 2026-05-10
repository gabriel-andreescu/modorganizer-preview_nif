#pragma once

#include "ShaderManager.h"

namespace nifly {
class NifFile;
class NiShader;
}

[[nodiscard]] ShaderManager::ShaderType classifyShaderType(
    const nifly::NifFile* nifFile,
    const nifly::NiShader* shader
);
