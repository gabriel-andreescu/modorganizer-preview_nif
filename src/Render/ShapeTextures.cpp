#include "Render/ShapeTextures.h"
#include "Nif/Fo4Material.h"
#include "Nif/ShaderUtils.h"
#include "Textures/Manager.h"
#include "Textures/PreviewTexture.h"

#include <QDebug>
#include <QOpenGLShaderProgram>
#include <QStringList>

#include <nifly/NifFile.hpp>

#include <algorithm>
#include <string>

namespace {
PreviewTexture* FallbackTexture(TextureManager* a_textureManager, const TextureFallback a_fallback) {
    switch (a_fallback) {
        case TextureFallback::kError:      return a_textureManager->GetErrorTexture();
        case TextureFallback::kFlatNormal: return a_textureManager->GetFlatNormalTexture();
        case TextureFallback::kBlack:      return a_textureManager->GetBlackTexture();
        case TextureFallback::kWhite:      return a_textureManager->GetWhiteTexture();
        case TextureFallback::kNone:       return nullptr;
    }

    return nullptr;
}

bool TextureFeatureEnabled(
    const std::array<PreviewTexture*, kTextureSlotCount>& a_textures,
    const std::array<bool, kTextureSlotCount>& a_loadedTextures,
    const OpenGLShapeTextureFeatureFlags& a_materialFlags,
    const TextureSlotDescriptor& a_descriptor,
    const TextureFeatureUniform& a_feature
) {
    const auto materialFlagEnabled = [&] {
        switch (a_feature.materialFlag) {
            case TextureFeatureMaterialFlag::kGlowMap:   return a_materialFlags.hasGlowMap;
            case TextureFeatureMaterialFlag::kHeightMap: return a_materialFlags.hasHeightMap;
            case TextureFeatureMaterialFlag::kNone:      return true;
        }

        return false;
    };

    switch (a_feature.condition) {
        case TextureFeatureCondition::kAssignedTexture: return a_textures[a_descriptor.slot] != nullptr;
        case TextureFeatureCondition::kLoadedTexture:   return a_loadedTextures[a_descriptor.slot];
        case TextureFeatureCondition::kMaterialFlagAndAssignedTexture:
            return materialFlagEnabled() && a_textures[a_descriptor.slot] != nullptr;
        case TextureFeatureCondition::kMaterialFlagAndLoadedTexture:
            return materialFlagEnabled() && a_loadedTextures[a_descriptor.slot];
    }

    return false;
}

} // namespace

void OpenGLShapeTextures::InitializeDescriptors(
    const nifly::NiShader* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    const bool a_isRefractionProxy
) {
    _slotDescriptors = TextureSlotDescriptors(a_shader, a_shaderType, a_isRefractionProxy);
}

void OpenGLShapeTextures::Load(
    nifly::NifFile* a_nifFile,
    nifly::NiShader* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    const bool a_isPBR,
    TextureManager* a_textureManager
) {
    if (!a_shader) {
        return;
    }

    const auto loadedFo4Material = a_shaderType == ShaderManager::kFO4Default
                                   && LoadFo4MaterialTextures(a_shader, a_textureManager);
    if (loadedFo4Material) {
        return;
    }

    if (auto* const effectShader = dynamic_cast<nifly::BSEffectShaderProperty*>(a_shader)) {
        LoadEffectShaderTextures(effectShader, a_shaderType, a_textureManager);
        if (a_shaderType == ShaderManager::kFO4EffectShader && !GetShaderMaterialPath(a_shader, ".bgem").isEmpty()) {
            LoadFo4EffectMaterialTextures(a_shader, a_textureManager);
        }
    } else if (a_shader->HasTextureSet()) {
        LoadTextureSetTextures(a_nifFile, a_shader, a_textureManager);
    }

    if (a_isPBR) {
        EnsurePBRTextureDefaults(a_textureManager);
    }
}

void OpenGLShapeTextures::UseDefaultTextures(TextureManager* a_textureManager) {
    AssignFallback(a_textureManager, kBaseMap, TextureFallback::kWhite);
    AssignFallback(a_textureManager, kNormalMap, TextureFallback::kFlatNormal);
}

void OpenGLShapeTextures::SetupUniforms(
    QOpenGLShaderProgram* a_program,
    const OpenGLShapeTextureFeatureFlags& a_materialFlags
) const {
    for (const auto& descriptor : _slotDescriptors) {
        for (std::size_t i = 0; i < descriptor.samplerUniformCount; ++i) {
            a_program->setUniformValue(descriptor.samplerUniforms[i].name, static_cast<int>(descriptor.slot + 1));
        }
    }

    for (const auto& descriptor : _slotDescriptors) {
        for (std::size_t i = 0; i < descriptor.featureUniformCount; ++i) {
            const auto& feature = descriptor.featureUniforms[i];
            a_program->setUniformValue(
                feature.name,
                static_cast<GLint>(
                    TextureFeatureEnabled(_textures, _loadedTextures, a_materialFlags, descriptor, feature)
                )
            );
        }
    }

    BindTextures();

    a_program->setUniformValue(
        "hasSourceTexture",
        static_cast<GLint>(_hasSourceTexture && _textures[kBaseMap] != nullptr)
    );
    a_program->setUniformValue(
        "hasGreyscaleMap",
        static_cast<GLint>(_hasGreyscaleMap && _textures[kGreyscaleMap] != nullptr)
    );
}

std::vector<TextureSlotState> OpenGLShapeTextures::SlotStates() const {
    std::vector<TextureSlotState> states;
    for (std::size_t slot = 0; slot < kTextureSlotCount; ++slot) {
        if (_requestedPaths[slot].isEmpty() && _textures[slot] == nullptr) {
            continue;
        }

        states.push_back({
            .descriptor = _slotDescriptors[slot],
            .requestedPath = _requestedPaths[slot],
            .loaded = _loadedTextures[slot],
            .fallback = _appliedFallbacks[slot],
        });
    }

    return states;
}

void OpenGLShapeTextures::LoadEffectShaderTextures(
    nifly::BSEffectShaderProperty* a_shader,
    const ShaderManager::ShaderType a_shaderType,
    TextureManager* a_textureManager
) {
    const auto sourceTexture = a_shader->sourceTexture.get();
    const auto greyscaleTexture = a_shader->greyscaleTexture.get();

    _hasSourceTexture = !sourceTexture.empty();
    _hasGreyscaleMap = !greyscaleTexture.empty();

    const auto assignEffectTexture = [&](const std::size_t a_slot, const std::string& a_texturePath) {
        const auto fallback = _slotDescriptors[a_slot].directFallback;
        _requestedPaths[a_slot] = QString::fromStdString(a_texturePath);
        if (a_texturePath.empty()) {
            AssignFallback(a_textureManager, a_slot, fallback.emptyPath);
            return;
        }

        if (auto* const texture = a_textureManager->GetTexture(a_texturePath)) {
            _textures[a_slot] = texture;
            _loadedTextures[a_slot] = true;
            return;
        }

        AssignFallback(a_textureManager, a_slot, fallback.failedLoad);
    };

    assignEffectTexture(kBaseMap, sourceTexture);
    assignEffectTexture(kGreyscaleMap, greyscaleTexture);

    if (a_shaderType != ShaderManager::kFO4EffectShader) {
        return;
    }

    assignEffectTexture(kNormalMap, a_shader->normalTexture.get());
    assignEffectTexture(kEnvironmentMap, a_shader->envMapTexture.get());
    assignEffectTexture(kEnvironmentMask, a_shader->envMaskTexture.get());
}

bool OpenGLShapeTextures::LoadFo4MaterialTextures(nifly::NiShader* a_shader, TextureManager* a_textureManager) {
    _materialPath = GetShaderMaterialPath(a_shader, ".bgsm");
    const auto textures = a_textureManager->GetFo4MaterialTextures(_materialPath);
    if (textures.isEmpty()) {
        return false;
    }

    const auto assignMaterialTexture = [&](const std::size_t a_slot, const Fo4Material::TextureIndex a_index) {
        const auto fallback = _slotDescriptors[a_slot].textureSetFallback;
        if (a_index >= textures.size() || textures[a_index].isEmpty()) {
            AssignFallback(a_textureManager, a_slot, fallback);
            return;
        }

        _requestedPaths[a_slot] = textures[a_index];
        _textures[a_slot] = a_textureManager->GetTexture(textures[a_index]);
        _loadedTextures[a_slot] = _textures[a_slot] != nullptr;
        if (!_textures[a_slot]) {
            AssignFallback(a_textureManager, a_slot, fallback);
        }
    };

    assignMaterialTexture(TextureSlot::kBaseMap, Fo4Material::kDiffuse);
    assignMaterialTexture(TextureSlot::kNormalMap, Fo4Material::kNormal);
    assignMaterialTexture(TextureSlot::kSpecularMap, Fo4Material::kSpecular);
    assignMaterialTexture(TextureSlot::kGreyscaleMap, Fo4Material::kGreyscale);
    assignMaterialTexture(TextureSlot::kEnvironmentMap, Fo4Material::kEnvironment);
    assignMaterialTexture(TextureSlot::kEnvironmentMask, Fo4Material::kGlowOrEnvironmentMask);
    assignMaterialTexture(TextureSlot::kGlowMap, Fo4Material::kGlowOrEnvironmentMask);
    return true;
}

bool OpenGLShapeTextures::LoadFo4EffectMaterialTextures(nifly::NiShader* a_shader, TextureManager* a_textureManager) {
    _materialPath = GetShaderMaterialPath(a_shader, ".bgem");
    const auto textures = a_textureManager->GetFo4MaterialTextures(_materialPath);
    if (textures.isEmpty()) {
        return false;
    }

    const auto assignMaterialTexture = [&](const std::size_t a_slot, const int a_index) {
        if (a_index >= textures.size() || textures[a_index].isEmpty()) {
            return;
        }

        if (auto* const texture = a_textureManager->GetTexture(textures[a_index])) {
            _requestedPaths[a_slot] = textures[a_index];
            _textures[a_slot] = texture;
            _loadedTextures[a_slot] = true;
            _appliedFallbacks[a_slot] = TextureFallback::kNone;
        }
    };

    assignMaterialTexture(TextureSlot::kBaseMap, 0);
    assignMaterialTexture(TextureSlot::kGreyscaleMap, 1);
    assignMaterialTexture(TextureSlot::kEnvironmentMap, 2);
    assignMaterialTexture(TextureSlot::kNormalMap, 3);
    assignMaterialTexture(TextureSlot::kEnvironmentMask, 4);
    return true;
}

void OpenGLShapeTextures::LoadTextureSetTextures(
    nifly::NifFile* a_nifFile,
    nifly::NiShader* a_shader,
    TextureManager* a_textureManager
) {
    const auto* const textureSetRef = a_shader->TextureSetRef();
    auto* const textureSet = a_nifFile->GetHeader().GetBlock(textureSetRef);

    if (!textureSet) {
        qWarning("Skipping missing shader texture set");
    }

    const auto nifTextureCount = textureSet ? static_cast<std::size_t>(textureSet->textures.size()) : 0;
    const auto textureCount = std::min(nifTextureCount, _textures.size());
    if (textureSet && nifTextureCount > _textures.size()) {
        qWarning("Skipping %zu unsupported texture slots", nifTextureCount - _textures.size());
    }

    for (std::size_t i = 0; i < textureCount; i++) {
        const auto textureIndex = static_cast<std::uint32_t>(i);
        if (const auto texturePath = textureSet->textures[textureIndex].get(); !texturePath.empty()) {
            _requestedPaths[i] = QString::fromStdString(texturePath);
            _textures[i] = a_textureManager->GetTexture(texturePath);
            _loadedTextures[i] = _textures[i] != nullptr;
        }

        if (_textures[i] == nullptr) {
            AssignFallback(a_textureManager, i, _slotDescriptors[i].textureSetFallback);
        }
    }
}

void OpenGLShapeTextures::AssignFallback(
    TextureManager* a_textureManager,
    const std::size_t a_textureSlot,
    const TextureFallback a_fallback
) {
    _textures[a_textureSlot] = FallbackTexture(a_textureManager, a_fallback);
    _loadedTextures[a_textureSlot] = false;
    _appliedFallbacks[a_textureSlot] = _textures[a_textureSlot] ? a_fallback : TextureFallback::kNone;
}

void OpenGLShapeTextures::EnsurePBRTextureDefaults(TextureManager* a_textureManager) {
    for (std::size_t slot = 0; slot < _textures.size(); ++slot) {
        if (!_textures[slot]) {
            AssignFallback(a_textureManager, slot, _slotDescriptors[slot].textureSetFallback);
        }
    }
}

void OpenGLShapeTextures::BindTextures() const {
    for (std::size_t i = 0; i < _textures.size(); i++) {
        if (_textures[i]) {
            _textures[i]->Bind(static_cast<int>(i + 1));
        }
    }
}
