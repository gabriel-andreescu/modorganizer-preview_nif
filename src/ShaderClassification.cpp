#include "ShaderClassification.h"
#include "NifExtensions.h"

#include <NifFile.hpp>

namespace {
bool isEffectShader(const nifly::NiShader* shader) {
    return dynamic_cast<const nifly::BSEffectShaderProperty*>(shader) != nullptr;
}

ShaderManager::ShaderType classifyNonFo4ShaderType(const nifly::NiShader* shader) {
    if (!shader) {
        return ShaderManager::None;
    }

    if (IsPBRLightingShader(shader)) {
        return ShaderManager::SKPBR;
    }
    if (isEffectShader(shader)) {
        return ShaderManager::SKEffectShader;
    }
    if (const auto* const bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(shader)) {
        if (bslsp->IsModelSpace()) {
            return ShaderManager::SKMSN;
        }
        if (bslsp->GetShaderType() == nifly::BSLSP_MULTILAYERPARALLAX) {
            return ShaderManager::SKMultilayer;
        }
    }
    if (shader->IsModelSpace()) {
        return ShaderManager::SKMSN;
    }

    return ShaderManager::SKDefault;
}
} // namespace

ShaderManager::ShaderType classifyShaderType(const nifly::NifFile* nifFile, const nifly::NiShader* shader) {
    if (!shader) {
        return ShaderManager::None;
    }

    if (nifFile && nifFile->GetHeader().GetVersion().IsFO4()) {
        return isEffectShader(shader) ? ShaderManager::FO4EffectShader : ShaderManager::FO4Default;
    }

    return classifyNonFo4ShaderType(shader);
}
