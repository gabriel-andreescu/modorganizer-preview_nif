#include "Render/ShaderClassification.h"
#include "Nif/ShaderUtils.h"

#include <nifly/NifFile.hpp>

namespace {
bool IsEffectShader(const nifly::NiShader* a_shader) {
    return dynamic_cast<const nifly::BSEffectShaderProperty*>(a_shader) != nullptr;
}

ShaderManager::ShaderType ClassifyNonFo4ShaderType(const nifly::NiShader* a_shader) {
    if (!a_shader) {
        return ShaderManager::kNone;
    }

    if (IsPBRLightingShader(a_shader)) {
        return ShaderManager::kSKPBR;
    }
    if (IsEffectShader(a_shader)) {
        return ShaderManager::kSKEffectShader;
    }
    if (const auto* const bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(a_shader)) {
        if (bslsp->IsModelSpace()) {
            return ShaderManager::kSKMSN;
        }
        if (bslsp->GetShaderType() == nifly::BSLSP_MULTILAYERPARALLAX) {
            return ShaderManager::kSKMultilayer;
        }
    }
    if (a_shader->IsModelSpace()) {
        return ShaderManager::kSKMSN;
    }

    return ShaderManager::kSKDefault;
}
} // namespace

ShaderManager::ShaderType ClassifyShaderType(const nifly::NifFile* a_nifFile, const nifly::NiShader* a_shader) {
    if (!a_shader) {
        return ShaderManager::kNone;
    }

    if (a_nifFile && a_nifFile->GetHeader().GetVersion().IsFO4()) {
        return IsEffectShader(a_shader) ? ShaderManager::kFO4EffectShader : ShaderManager::kFO4Default;
    }

    return ClassifyNonFo4ShaderType(a_shader);
}
