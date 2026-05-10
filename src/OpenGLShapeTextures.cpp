#include "OpenGLShapeTextures.h"
#include "Fo4Material.h"
#include "NifShaderUtils.h"
#include "PreviewTexture.h"
#include "TextureManager.h"

#include <QDebug>
#include <QOpenGLShaderProgram>
#include <QStringList>

#include <NifFile.hpp>

#include <algorithm>
#include <string>

namespace {
PreviewTexture* fallbackTexture(TextureManager* textureManager, const TextureFallback fallback) {
    switch (fallback) {
        case TextureFallback::Error:      return textureManager->getErrorTexture();
        case TextureFallback::FlatNormal: return textureManager->getFlatNormalTexture();
        case TextureFallback::Black:      return textureManager->getBlackTexture();
        case TextureFallback::White:      return textureManager->getWhiteTexture();
        case TextureFallback::None:       return nullptr;
    }

    return nullptr;
}

PreviewTexture* loadEffectTexture(
    TextureManager* textureManager,
    const std::string& texturePath,
    PreviewTexture* emptyFallback,
    PreviewTexture* missingFallback,
    bool& loadedTexture
) {
    loadedTexture = false;

    if (texturePath.empty()) {
        return emptyFallback;
    }

    if (auto* texture = textureManager->getTexture(texturePath)) {
        loadedTexture = true;
        return texture;
    }

    return missingFallback;
}

bool textureFeatureEnabled(
    const std::array<PreviewTexture*, TextureSlotCount>& textures,
    const std::array<bool, TextureSlotCount>& loadedTextures,
    const OpenGLShapeTextureFeatureFlags& materialFlags,
    const TextureSlotDescriptor& descriptor,
    const TextureFeatureUniform& feature
) {
    const auto materialFlagEnabled = [&] {
        switch (feature.materialFlag) {
            case TextureFeatureMaterialFlag::GlowMap:   return materialFlags.hasGlowMap;
            case TextureFeatureMaterialFlag::HeightMap: return materialFlags.hasHeightMap;
            case TextureFeatureMaterialFlag::None:      return true;
        }

        return false;
    };

    switch (feature.condition) {
        case TextureFeatureCondition::AssignedTexture: return textures[descriptor.slot] != nullptr;
        case TextureFeatureCondition::LoadedTexture:   return loadedTextures[descriptor.slot];
        case TextureFeatureCondition::MaterialFlagAndAssignedTexture:
            return materialFlagEnabled() && textures[descriptor.slot] != nullptr;
        case TextureFeatureCondition::MaterialFlagAndLoadedTexture:
            return materialFlagEnabled() && loadedTextures[descriptor.slot];
    }

    return false;
}

} // namespace

void OpenGLShapeTextures::initializeDescriptors(
    const nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const bool isRefractionProxy
) {
    m_SlotDescriptors = textureSlotDescriptors(shader, shaderType, isRefractionProxy);
}

void OpenGLShapeTextures::load(
    nifly::NifFile* nifFile,
    nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const bool isPBR,
    TextureManager* textureManager
) {
    if (!shader) {
        return;
    }

    const auto loadedFo4Material = shaderType
                                   == ShaderManager::FO4Default
                                   && loadFo4MaterialTextures(shader, textureManager);
    if (loadedFo4Material) {
        return;
    }

    if (auto* const effectShader = dynamic_cast<nifly::BSEffectShaderProperty*>(shader)) {
        loadEffectShaderTextures(effectShader, shaderType, textureManager);
        if (shaderType == ShaderManager::FO4EffectShader && !GetShaderMaterialPath(shader, ".bgem").isEmpty()) {
            loadFo4EffectMaterialTextures(shader, textureManager);
        }
    } else if (shader->HasTextureSet()) {
        loadTextureSetTextures(nifFile, shader, textureManager);
    }

    if (isPBR) {
        ensurePBRTextureDefaults(textureManager);
    }
}

void OpenGLShapeTextures::useDefaultTextures(TextureManager* textureManager) {
    m_Textures[BaseMap] = textureManager->getWhiteTexture();
    m_Textures[NormalMap] = textureManager->getFlatNormalTexture();
}

void OpenGLShapeTextures::setupUniforms(
    QOpenGLShaderProgram* program,
    const OpenGLShapeTextureFeatureFlags& materialFlags
) const {
    for (const auto& descriptor : m_SlotDescriptors) {
        for (std::size_t i = 0; i < descriptor.samplerUniformCount; ++i) {
            program->setUniformValue(descriptor.samplerUniforms[i].name, static_cast<int>(descriptor.slot + 1));
        }
    }

    for (const auto& descriptor : m_SlotDescriptors) {
        for (std::size_t i = 0; i < descriptor.featureUniformCount; ++i) {
            const auto& feature = descriptor.featureUniforms[i];
            program->setUniformValue(
                feature.name,
                textureFeatureEnabled(m_Textures, m_LoadedTextures, materialFlags, descriptor, feature)
            );
        }
    }

    bindTextures();

    program->setUniformValue("hasSourceTexture", m_HasSourceTexture && m_Textures[BaseMap] != nullptr);
    program->setUniformValue("hasGreyscaleMap", m_HasGreyscaleMap && m_Textures[GreyscaleMap] != nullptr);
}

void OpenGLShapeTextures::loadEffectShaderTextures(
    nifly::BSEffectShaderProperty* shader,
    const ShaderManager::ShaderType shaderType,
    TextureManager* textureManager
) {
    const auto sourceTexture = shader->sourceTexture.get();
    const auto greyscaleTexture = shader->greyscaleTexture.get();

    m_HasSourceTexture = !sourceTexture.empty();
    m_HasGreyscaleMap = !greyscaleTexture.empty();

    const auto assignEffectTexture = [&](const std::size_t slot, const std::string& texturePath) {
        const auto fallback = m_SlotDescriptors[slot].directFallback;
        bool loadedTexture = false;
        auto* const texture = loadEffectTexture(
            textureManager,
            texturePath,
            fallbackTexture(textureManager, fallback.emptyPath),
            fallbackTexture(textureManager, fallback.failedLoad),
            loadedTexture
        );
        m_Textures[slot] = texture;
        m_LoadedTextures[slot] = loadedTexture;
    };

    assignEffectTexture(BaseMap, sourceTexture);
    assignEffectTexture(GreyscaleMap, greyscaleTexture);

    if (shaderType != ShaderManager::FO4EffectShader) {
        return;
    }

    assignEffectTexture(NormalMap, shader->normalTexture.get());
    assignEffectTexture(EnvironmentMap, shader->envMapTexture.get());
    assignEffectTexture(EnvironmentMask, shader->envMaskTexture.get());
}

bool OpenGLShapeTextures::loadFo4MaterialTextures(nifly::NiShader* shader, TextureManager* textureManager) {
    const auto textures = textureManager->getFo4MaterialTextures(GetShaderMaterialPath(shader, ".bgsm"));
    if (textures.isEmpty()) {
        return false;
    }

    const auto assignMaterialTexture = [&](const std::size_t slot, const Fo4Material::TextureIndex index) {
        if (index >= textures.size() || textures[index].isEmpty()) {
            assignMissingTexture(textureManager, slot);
            return;
        }

        m_Textures[slot] = textureManager->getTexture(textures[index]);
        m_LoadedTextures[slot] = m_Textures[slot] != nullptr;
        if (!m_Textures[slot]) {
            assignMissingTexture(textureManager, slot);
        }
    };

    assignMaterialTexture(TextureSlot::BaseMap, Fo4Material::Diffuse);
    assignMaterialTexture(TextureSlot::NormalMap, Fo4Material::Normal);
    assignMaterialTexture(TextureSlot::SpecularMap, Fo4Material::Specular);
    assignMaterialTexture(TextureSlot::GreyscaleMap, Fo4Material::Greyscale);
    assignMaterialTexture(TextureSlot::EnvironmentMap, Fo4Material::Environment);
    assignMaterialTexture(TextureSlot::EnvironmentMask, Fo4Material::GlowOrEnvironmentMask);
    assignMaterialTexture(TextureSlot::GlowMap, Fo4Material::GlowOrEnvironmentMask);
    return true;
}

bool OpenGLShapeTextures::loadFo4EffectMaterialTextures(nifly::NiShader* shader, TextureManager* textureManager) {
    const auto textures = textureManager->getFo4MaterialTextures(GetShaderMaterialPath(shader, ".bgem"));
    if (textures.isEmpty()) {
        return false;
    }

    const auto assignMaterialTexture = [&](const std::size_t slot, const int index) {
        if (index >= textures.size() || textures[index].isEmpty()) {
            return;
        }

        if (auto* const texture = textureManager->getTexture(textures[index])) {
            m_Textures[slot] = texture;
            m_LoadedTextures[slot] = true;
        }
    };

    assignMaterialTexture(TextureSlot::BaseMap, 0);
    assignMaterialTexture(TextureSlot::GreyscaleMap, 1);
    assignMaterialTexture(TextureSlot::EnvironmentMap, 2);
    assignMaterialTexture(TextureSlot::NormalMap, 3);
    assignMaterialTexture(TextureSlot::EnvironmentMask, 4);
    return true;
}

void OpenGLShapeTextures::loadTextureSetTextures(
    nifly::NifFile* nifFile,
    nifly::NiShader* shader,
    TextureManager* textureManager
) {
    auto* const textureSetRef = shader->TextureSetRef();
    auto* const textureSet = nifFile->GetHeader().GetBlock(textureSetRef);

    if (!textureSet) {
        qWarning("Skipping missing shader texture set");
    }

    const auto nifTextureCount = textureSet ? static_cast<std::size_t>(textureSet->textures.size()) : 0;
    const auto textureCount = std::min(nifTextureCount, m_Textures.size());
    if (textureSet && nifTextureCount > m_Textures.size()) {
        qWarning("Skipping %zu unsupported texture slots", nifTextureCount - m_Textures.size());
    }

    for (std::size_t i = 0; i < textureCount; i++) {
        const auto textureIndex = static_cast<std::uint32_t>(i);
        if (auto texturePath = textureSet->textures[textureIndex].get(); !texturePath.empty()) {
            m_Textures[i] = textureManager->getTexture(texturePath);
            m_LoadedTextures[i] = m_Textures[i] != nullptr;
        }

        if (m_Textures[i] == nullptr) {
            assignMissingTexture(textureManager, i);
        }
    }
}

void OpenGLShapeTextures::assignMissingTexture(TextureManager* textureManager, const std::size_t textureSlot) {
    m_Textures[textureSlot] = fallbackTexture(textureManager, m_SlotDescriptors[textureSlot].textureSetFallback);
}

void OpenGLShapeTextures::ensurePBRTextureDefaults(TextureManager* textureManager) {
    for (std::size_t slot = 0; slot < m_Textures.size(); ++slot) {
        if (!m_Textures[slot]) {
            m_Textures[slot] = fallbackTexture(textureManager, m_SlotDescriptors[slot].textureSetFallback);
        }
    }
}

void OpenGLShapeTextures::bindTextures() const {
    for (std::size_t i = 0; i < m_Textures.size(); i++) {
        if (m_Textures[i]) {
            m_Textures[i]->bind(static_cast<int>(i + 1));
        }
    }
}
