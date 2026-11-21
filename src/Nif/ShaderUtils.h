#pragma once

#include "Nif/ShaderFlags.h"
#include "Textures/Slots.h"

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <nifly/NifFile.hpp>

#include <cstddef>
#include <cstdint>

inline QString GetShaderTexturePath(nifly::BSShaderTextureSet* a_textureSet, const std::size_t a_slot) {
    if (!a_textureSet || a_slot >= a_textureSet->textures.size()) {
        return {};
    }

    return QDir::fromNativeSeparators(
        QString::fromStdString(a_textureSet->textures[static_cast<std::uint32_t>(a_slot)].get())
    )
        .trimmed();
}

inline bool TexturePathsEqual(const QString& a_left, const QString& a_right) {
    return QString::compare(a_left, a_right, Qt::CaseInsensitive) == 0;
}

inline bool IsPBRLightingShader(const nifly::NiShader* a_shader) {
    const auto* const bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(a_shader);
    return (bslsp != nullptr) && HasFlag(bslsp->shaderFlags2, SLSF2::kPBR);
}

inline QString GetShaderMaterialPath(const nifly::NiShader* a_shader, const QString& a_suffix) {
    if (!a_shader) {
        return {};
    }

    const auto name = QString::fromStdString(a_shader->name.get()).trimmed();
    return name.endsWith(a_suffix, Qt::CaseInsensitive) ? name : QString {};
}

inline bool IsNormalLikeTexturePath(const QString& a_texturePath) {
    const auto stem = QFileInfo(a_texturePath).completeBaseName().toLower();
    return stem.endsWith("_n") || stem.endsWith("_msn");
}

inline bool IsRefractionDistortionProxy(const nifly::NifFile* a_nifFile, nifly::NiShape* a_niShape) {
    if (!a_nifFile || !a_niShape || a_nifFile->GetAlphaProperty(a_niShape)) {
        return false;
    }

    auto* const shader = dynamic_cast<nifly::BSLightingShaderProperty*>(a_nifFile->GetShader(a_niShape));
    if (!shader
        || ((shader->shaderFlags1 & (SLSF1::kRefraction | SLSF1::kFireRefraction)) == 0U)
        || !shader->HasTextureSet()) {
        return false;
    }

    auto* const textureSet = a_nifFile->GetHeader().GetBlock(shader->TextureSetRef());
    const auto baseTexture = GetShaderTexturePath(textureSet, TextureSlot::kBaseMap);
    const auto normalTexture = GetShaderTexturePath(textureSet, TextureSlot::kNormalMap);

    return !baseTexture.isEmpty()
           && ((!normalTexture.isEmpty() && TexturePathsEqual(baseTexture, normalTexture))
               || IsNormalLikeTexturePath(baseTexture));
}
