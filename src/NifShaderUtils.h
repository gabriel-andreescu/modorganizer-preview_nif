#pragma once

#include "NifShaderFlags.h"
#include "TextureSlots.h"

#include <NifFile.hpp>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <cstddef>
#include <cstdint>

inline QString GetShaderTexturePath(nifly::BSShaderTextureSet* textureSet, const std::size_t slot) {
    if (!textureSet || slot >= textureSet->textures.size()) {
        return {};
    }

    return QDir::fromNativeSeparators(
        QString::fromStdString(textureSet->textures[static_cast<std::uint32_t>(slot)].get())
    )
        .trimmed();
}

inline bool TexturePathsEqual(const QString& left, const QString& right) {
    return QString::compare(left, right, Qt::CaseInsensitive) == 0;
}

inline bool IsPBRLightingShader(const nifly::NiShader* shader) {
    const auto bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(shader);
    return bslsp && HasFlag(bslsp->shaderFlags2, SLSF2::PBR);
}

inline QString GetShaderMaterialPath(const nifly::NiShader* shader, const QString& suffix) {
    if (!shader) {
        return {};
    }

    const auto name = QString::fromStdString(shader->name.get()).trimmed();
    return name.endsWith(suffix, Qt::CaseInsensitive) ? name : QString {};
}

inline bool IsNormalLikeTexturePath(const QString& texturePath) {
    const auto stem = QFileInfo(texturePath).completeBaseName().toLower();
    return stem.endsWith("_n") || stem.endsWith("_msn");
}

inline bool IsRefractionDistortionProxy(const nifly::NifFile* nifFile, nifly::NiShape* niShape) {
    if (!nifFile || !niShape || nifFile->GetAlphaProperty(niShape)) {
        return false;
    }

    const auto shader = dynamic_cast<nifly::BSLightingShaderProperty*>(nifFile->GetShader(niShape));
    if (!shader || !(shader->shaderFlags1 & (SLSF1::Refraction | SLSF1::FireRefraction)) || !shader->HasTextureSet()) {
        return false;
    }

    const auto textureSet = nifFile->GetHeader().GetBlock(shader->TextureSetRef());
    const auto baseTexture = GetShaderTexturePath(textureSet, TextureSlot::BaseMap);
    const auto normalTexture = GetShaderTexturePath(textureSet, TextureSlot::NormalMap);

    return !baseTexture.isEmpty()
           && ((!normalTexture.isEmpty() && TexturePathsEqual(baseTexture, normalTexture))
               || IsNormalLikeTexturePath(baseTexture));
}
