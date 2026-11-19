#include "Textures/Source.h"
#include "Data/GameFiles.h"
#include "Nif/Fo4Material.h"
#include "Nif/ShaderUtils.h"
#include "Nif/ShapeVisibility.h"
#include "Render/ShaderClassification.h"
#include "Textures/Loader.h"
#include "Textures/SlotDescriptors.h"

#include <nifly/NifFile.hpp>

#include <QDir>
#include <QMap>
#include <QObject>
#include <QSet>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <ranges>
#include <uibase/imodlist.h>
#include <uibase/imoinfo.h>
#include <utility>

namespace {
void AppendUnique(QStringList& a_values, const QString& a_value) {
    if (!a_value.isEmpty() && !a_values.contains(a_value, Qt::CaseInsensitive)) {
        a_values.append(a_value);
    }
}

struct ShaderTextureReferences {
    QVector<TextureReference>& references;
    QSet<QString>& seenPaths;
    const nifly::NiShader* shader;
    ShaderManager::ShaderType shaderType;
};

void AppendTextureReference(
    const ShaderTextureReferences& a_target,
    const int a_slot,
    const QString& a_texturePath,
    const bool a_isRefractionProxy = false
) {
    const auto path = NormalizeTextureDataPath(a_texturePath);
    const auto key = TextureDataPathKey(path);
    if (path.isEmpty()) {
        return;
    }

    const auto slotName = TextureSlotDisplayName(
        a_target.shader,
        a_target.shaderType,
        static_cast<std::size_t>(a_slot),
        a_isRefractionProxy
    );

    if (a_target.seenPaths.contains(key)) {
        if (a_isRefractionProxy && a_slot == TextureSlot::kBaseMap) {
            const auto reference = std::ranges::find_if(a_target.references, [&](const auto& a_reference) {
                return a_reference.key == key;
            });
            if (reference != a_target.references.end()) {
                reference->slot = a_slot;
                reference->slotName = slotName;
            }
        }
        return;
    }

    a_target.seenPaths.insert(key);
    a_target.references.push_back({.slot = a_slot, .slotName = slotName, .path = path, .key = key});
}

void AppendEffectShaderTextureReferences(
    const ShaderTextureReferences& a_target,
    const nifly::BSEffectShaderProperty* a_effectShader
) {
    AppendTextureReference(
        a_target,
        TextureSlot::kBaseMap,
        QString::fromStdString(a_effectShader->sourceTexture.get())
    );
    AppendTextureReference(
        a_target,
        TextureSlot::kGreyscaleMap,
        QString::fromStdString(a_effectShader->greyscaleTexture.get())
    );

    if (a_target.shaderType != ShaderManager::kFO4EffectShader) {
        return;
    }

    AppendTextureReference(
        a_target,
        TextureSlot::kNormalMap,
        QString::fromStdString(a_effectShader->normalTexture.get())
    );
    AppendTextureReference(
        a_target,
        TextureSlot::kEnvironmentMap,
        QString::fromStdString(a_effectShader->envMapTexture.get())
    );
    AppendTextureReference(
        a_target,
        TextureSlot::kEnvironmentMask,
        QString::fromStdString(a_effectShader->envMaskTexture.get())
    );
}

void AppendFo4MaterialTextureReferences(const ShaderTextureReferences& a_target, const GameFiles& a_gameFiles) {
    const bool isEffectShader = a_target.shaderType == ShaderManager::kFO4EffectShader;
    const auto materialPath = GetShaderMaterialPath(a_target.shader, isEffectShader ? ".bgem" : ".bgsm");
    if (materialPath.isEmpty()) {
        return;
    }

    const TextureLoader loader(a_gameFiles);
    const auto material = Fo4Material::Read(
        loader.LoadDataFile(Fo4Material::NormalizeMaterialDataPath(materialPath)).data
    );
    if (!material.valid) {
        return;
    }

    const auto& textures = material.textures;
    AppendTextureReference(a_target, TextureSlot::kBaseMap, textures.base);
    AppendTextureReference(a_target, TextureSlot::kNormalMap, textures.normal);
    AppendTextureReference(a_target, TextureSlot::kSpecularMap, textures.specular);
    AppendTextureReference(a_target, TextureSlot::kGreyscaleMap, textures.greyscale);
    AppendTextureReference(a_target, TextureSlot::kEnvironmentMap, textures.environment);
    AppendTextureReference(a_target, TextureSlot::kEnvironmentMask, textures.environmentMask);
    AppendTextureReference(a_target, TextureSlot::kGlowMap, textures.glow);
}

QVector<TextureReference> TextureReferencesFor(const GameFiles& a_gameFiles, const nifly::NifFile* a_nifFile) {
    QVector<TextureReference> references;
    QSet<QString> seenPaths;
    if (!a_nifFile) {
        return references;
    }

    for (auto* shape : a_nifFile->GetShapes()) {
        if (!shape) {
            continue;
        }
        if (const auto visibility = ClassifyShapeVisibility(*a_nifFile, shape);
            visibility == ShapeVisibility::kHidden || visibility == ShapeVisibility::kInactiveSwitchChild) {
            continue;
        }

        auto* const shader = a_nifFile->GetShader(shape);
        if (!shader) {
            continue;
        }

        const auto isRefractionProxy = IsRefractionDistortionProxy(a_nifFile, shape);
        const auto shaderType = ClassifyShaderType(a_nifFile, shader);
        const ShaderTextureReferences target {
            .references = references,
            .seenPaths = seenPaths,
            .shader = shader,
            .shaderType = shaderType,
        };

        if (shaderType == ShaderManager::kFO4Default || shaderType == ShaderManager::kFO4EffectShader) {
            AppendFo4MaterialTextureReferences(target, a_gameFiles);
        }

        if (const auto* const effectShader = dynamic_cast<nifly::BSEffectShaderProperty*>(shader)) {
            AppendEffectShaderTextureReferences(target, effectShader);
        }

        if (!shader->HasTextureSet()) {
            continue;
        }

        auto* const textureSet = a_nifFile->GetHeader().GetBlock(shader->TextureSetRef());
        if (!textureSet) {
            continue;
        }

        const auto textureCount = std::min<std::size_t>(textureSet->textures.size(), kTextureSlotCount);
        for (std::size_t i = 0; i < textureCount; i++) {
            AppendTextureReference(
                target,
                static_cast<int>(i),
                QString::fromStdString(textureSet->textures[static_cast<std::uint32_t>(i)].get()),
                isRefractionProxy
            );
        }
    }

    return references;
}

QString CoverageDisplayName(const QString& a_name, const qsizetype a_covered, const qsizetype a_total) {
    return QObject::tr("%1 (%2/%3)").arg(a_name).arg(a_covered).arg(a_total);
}

TextureSourceProvider MakeProvider(
    const TextureSourceProviderKind a_kind,
    const QString& a_sourceName,
    const QString& a_displayName,
    const qsizetype a_coveredTextureCount,
    const qsizetype a_totalTextureCount
) {
    return {
        .displayName = CoverageDisplayName(a_displayName, a_coveredTextureCount, a_totalTextureCount),
        .kind = a_kind,
        .sourceName = a_sourceName,
        .coveredTextureCount = a_coveredTextureCount,
        .totalTextureCount = a_totalTextureCount,
    };
}

QString ModDisplayName(const MOBase::IOrganizer* a_organizer, const QString& a_modName) {
    const auto* const modList = a_organizer ? a_organizer->modList() : nullptr;
    const auto displayName = modList ? modList->displayName(a_modName) : QString();
    return displayName.isEmpty() ? a_modName : displayName;
}

// Matches MO2's mod list: highest priority first, with the overwrite folder above every mod.
void OrderModsByPriority(QStringList& a_mods, const MOBase::IOrganizer* a_organizer) {
    const auto* const modList = a_organizer ? a_organizer->modList() : nullptr;
    if (!modList) {
        return;
    }

    const auto rank = [&](const QString& a_mod) {
        return a_mod.compare(QStringLiteral("overwrite"), Qt::CaseInsensitive) == 0 ? std::numeric_limits<int>::max()
                                                                                    : modList->priority(a_mod);
    };
    std::ranges::stable_sort(a_mods, std::ranges::greater {}, rank);
}
} // namespace

QString NormalizeTextureDataPath(QString a_path) {
    a_path = QDir::fromNativeSeparators(a_path).trimmed();
    while (a_path.startsWith('/')) {
        a_path.remove(0, 1);
    }
    if (a_path.startsWith("data/", Qt::CaseInsensitive)) {
        a_path.remove(0, 5);
    }

    const auto textureIndex = a_path.indexOf("textures/", 0, Qt::CaseInsensitive);
    if (textureIndex > 0) {
        a_path.remove(0, textureIndex);
    } else if (
        textureIndex < 0
        && !a_path.startsWith("shaders/", Qt::CaseInsensitive)
        && a_path.endsWith(".dds", Qt::CaseInsensitive)
    ) {
        a_path.prepend("textures/");
    }

    return a_path;
}

QStringList TextureDataPathVariants(const QString& a_path) {
    QStringList paths;

    const auto normalized = NormalizeTextureDataPath(a_path);
    AppendUnique(paths, normalized);

    auto original = QDir::fromNativeSeparators(a_path).trimmed();
    while (original.startsWith('/')) {
        original.remove(0, 1);
    }
    AppendUnique(paths, original);

    if (original.startsWith("data/", Qt::CaseInsensitive)) {
        original.remove(0, 5);
        AppendUnique(paths, original);
    }

    return paths;
}

QString TextureDataPathKey(const QString& a_path) {
    return NormalizeTextureDataPath(a_path).toLower();
}

TextureFiles FindTextureFiles(const GameFiles& a_gameFiles, const QString& a_texturePath) {
    for (const auto& path : TextureDataPathVariants(a_texturePath)) {
        if (auto sources = a_gameFiles.Sources(path); !sources.isEmpty()) {
            return {.dataPath = path, .sources = std::move(sources)};
        }
    }
    return {.dataPath = NormalizeTextureDataPath(a_texturePath), .sources = {}};
}

bool ProviderSupplies(const TextureSourceProvider& a_provider, const DataFileSource& a_source) {
    if (a_source.hidden) {
        return false;
    }

    switch (a_provider.kind) {
        case TextureSourceProviderKind::kMod:      return a_source.modName == a_provider.sourceName;
        case TextureSourceProviderKind::kGameData: return a_source.modName.isEmpty();
        case TextureSourceProviderKind::kAuto:     return true;
    }
    return false;
}

qsizetype SourceCopyIndex(const TextureSourceProvider& a_provider, const QVector<DataFileSource>& a_copies) {
    for (qsizetype i = 0; i < a_copies.size(); ++i) {
        if (ProviderSupplies(a_provider, a_copies[i])) {
            return i;
        }
    }
    // Hidden copies follow the visible ones, so a hidden first copy means the game sees none.
    return !a_copies.isEmpty() && !a_copies.front().hidden ? 0 : -1;
}

namespace {
void AppendSourceProviders(
    TextureSourceSet& a_sourceSet,
    const MOBase::IOrganizer* a_organizer,
    const GameFiles& a_gameFiles
) {
    QMap<QString, QSet<QString>> covered;
    QSet<QString> gameCovered;
    for (const auto& reference : a_sourceSet.references) {
        for (const auto& source : FindTextureFiles(a_gameFiles, reference.path).sources) {
            if (source.modName.isEmpty()) {
                gameCovered.insert(reference.key);
            } else {
                covered[source.modName].insert(reference.key);
            }
        }
    }

    auto modOrder = covered.keys();
    OrderModsByPriority(modOrder, a_organizer);
    const auto total = a_sourceSet.references.size();
    for (const auto& modName : modOrder) {
        a_sourceSet.providers.push_back(MakeProvider(
            TextureSourceProviderKind::kMod,
            modName,
            ModDisplayName(a_organizer, modName),
            covered[modName].size(),
            total
        ));
    }
    if (!gameCovered.isEmpty()) {
        a_sourceSet.providers.push_back(
            MakeProvider(TextureSourceProviderKind::kGameData, {}, QObject::tr("Game Data"), gameCovered.size(), total)
        );
    }
}
} // namespace

TextureSourceSet TextureSourceResolver::Resolve(
    const MOBase::IOrganizer* a_organizer,
    const GameFiles& a_gameFiles,
    const nifly::NifFile* a_nifFile
) {
    TextureSourceSet sourceSet;
    sourceSet.references = TextureReferencesFor(a_gameFiles, a_nifFile);
    const auto total = sourceSet.references.size();
    sourceSet.providers.push_back({
        .displayName = QObject::tr("Winning files"),
        .kind = TextureSourceProviderKind::kAuto,
        .sourceName = {},
        .coveredTextureCount = total,
        .totalTextureCount = total,
    });

    if (!sourceSet.references.isEmpty()) {
        AppendSourceProviders(sourceSet, a_organizer, a_gameFiles);
    }
    return sourceSet;
}
