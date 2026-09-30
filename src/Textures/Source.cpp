#include "Textures/Source.h"
#include "Data/ArchiveAccess.h"
#include "Data/MoDataPaths.h"
#include "Nif/Fo4Material.h"
#include "Nif/ShaderUtils.h"
#include "Render/ShaderClassification.h"
#include "Textures/Loader.h"
#include "Textures/SlotDescriptors.h"

#include <nifly/NifFile.hpp>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QObject>
#include <QSet>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <uibase/imodinterface.h>
#include <uibase/imodlist.h>
#include <uibase/imoinfo.h>
#include <uibase/iplugingame.h>
#include <utility>

#include <libbsarch/bs_archive.h>

namespace {
struct TextureProviderBuilder {
    QString sourceName;
    QString sourcePath;
    QString displayName;
    QStringList archivePaths;
    QStringList coveredTextureKeys;
};

struct TextureSlotSummary {
    QString name;
    int count = 0;
};

void AppendUnique(QStringList& a_values, const QString& a_value) {
    if (!a_value.isEmpty() && !a_values.contains(a_value, Qt::CaseInsensitive)) {
        a_values.append(a_value);
    }
}

bool LoadArchivePath(libbsarch::bs_archive& a_archive, const QString& a_archivePath) {
    QString error;
    if (ArchiveAccess::LoadArchive(a_archive, a_archivePath, &error)) {
        return true;
    }

    qWarning(
        "Failed to load BSA archive '%s' while resolving texture sources: %s",
        qUtf8Printable(a_archivePath),
        qUtf8Printable(error)
    );
    return false;
}

bool ArchiveContainsTexture(libbsarch::bs_archive& a_archive, const QString& a_texturePath) {
    QString errorPath;
    QString error;
    if (ArchiveAccess::ContainsDataPath(a_archive, a_texturePath, &errorPath, &error)) {
        return true;
    }

    if (!errorPath.isEmpty()) {
        qWarning(
            "Failed to inspect BSA archive texture path '%s': %s",
            qUtf8Printable(errorPath),
            qUtf8Printable(error)
        );
    }

    return false;
}

void AppendArchiveCoverage(
    QStringList& a_coveredTextureKeys,
    const QStringList& a_archivePaths,
    const QVector<TextureReference>& a_references
) {
    for (const auto& archivePath : a_archivePaths) {
        if (a_coveredTextureKeys.size() >= a_references.size()) {
            return;
        }

        libbsarch::bs_archive archive;
        if (archivePath.isEmpty() || !LoadArchivePath(archive, archivePath)) {
            continue;
        }

        for (const auto& reference : a_references) {
            if (a_coveredTextureKeys.contains(reference.key, Qt::CaseInsensitive)) {
                continue;
            }

            if (ArchiveContainsTexture(archive, reference.path)) {
                AppendUnique(a_coveredTextureKeys, reference.key);
            }
        }
    }
}

bool GameDataContainsTexture(const MOBase::IOrganizer* a_organizer, const QString& a_texturePath) {
    if (!a_organizer) {
        return false;
    }

    const auto* const game = a_organizer->managedGame();
    if (!game) {
        return false;
    }

    const auto dataPath = game->dataDirectory().absoluteFilePath(QDir::cleanPath(a_texturePath));
    return QFileInfo::exists(dataPath) && QFileInfo(dataPath).isFile();
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

void AppendFo4MaterialTextureReferences(const ShaderTextureReferences& a_target, MOBase::IOrganizer* a_organizer) {
    const bool isEffectShader = a_target.shaderType == ShaderManager::kFO4EffectShader;
    const auto materialPath = GetShaderMaterialPath(a_target.shader, isEffectShader ? ".bgem" : ".bgsm");
    if (materialPath.isEmpty()) {
        return;
    }

    const TextureLoader loader(a_organizer);
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

QVector<TextureReference> TextureReferencesFor(MOBase::IOrganizer* a_organizer, const nifly::NifFile* a_nifFile) {
    QVector<TextureReference> references;
    QSet<QString> seenPaths;
    if (!a_nifFile) {
        return references;
    }

    for (auto* shape : a_nifFile->GetShapes()) {
        if (!shape || ((shape->flags & TriShape::kHidden) != 0U)) {
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
            AppendFo4MaterialTextureReferences(target, a_organizer);
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

QVector<TextureSlotSummary> TextureSlotSummaries(const TextureSourceSet& a_sourceSet) {
    QVector<TextureSlotSummary> summaries;
    for (const auto& reference : a_sourceSet.references) {
        const auto existing = std::ranges::find_if(summaries, [&](const auto& a_summary) {
            return a_summary.name == reference.slotName;
        });
        if (existing != summaries.end()) {
            existing->count++;
            continue;
        }

        summaries.push_back({.name = reference.slotName, .count = 1});
    }

    return summaries;
}

QString TextureSlotSummaryLabel(const TextureSlotSummary& a_summary) {
    if (a_summary.count <= 1) {
        return a_summary.name;
    }

    return QObject::tr("%1 x%2").arg(a_summary.name).arg(a_summary.count);
}

QString TextureSlotSummaryList(const QVector<TextureSlotSummary>& a_summaries) {
    QStringList labels;
    for (const auto& summary : a_summaries) {
        labels << TextureSlotSummaryLabel(summary);
    }

    return labels.join(QObject::tr(", "));
}

TextureSourceProvider MakeProvider(
    TextureSourceProviderKind a_kind,
    const TextureProviderBuilder& a_builder,
    const qsizetype a_totalTextureCount
) {
    TextureSourceProvider provider;
    provider.kind = a_kind;
    provider.sourceName = a_builder.sourceName;
    provider.sourcePath = a_builder.sourcePath;
    provider.archivePaths = a_builder.archivePaths;
    provider.coveredTextureKeys = a_builder.coveredTextureKeys;
    provider.coveredTextureCount = a_builder.coveredTextureKeys.size();
    provider.totalTextureCount = a_totalTextureCount;
    provider.displayName = CoverageDisplayName(
        a_builder.displayName,
        provider.coveredTextureCount,
        provider.totalTextureCount
    );
    return provider;
}

void OrderModsByProfilePriority(QStringList& a_modOrder, MOBase::IModList* a_modList) {
    if (!a_modList || a_modOrder.size() < 2) {
        return;
    }

    std::ranges::stable_sort(a_modOrder, [&](const QString& a_lhs, const QString& a_rhs) {
        return a_modList->priority(a_lhs) > a_modList->priority(a_rhs);
    });
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

bool TextureProviderCoversPath(const TextureSourceProvider& a_provider, const QString& a_texturePath) {
    return a_provider.coveredTextureKeys.contains(TextureDataPathKey(a_texturePath), Qt::CaseInsensitive);
}

namespace {
TextureSourceProvider MakeAutoProvider(const qsizetype a_referenceCount) {
    TextureSourceProvider provider;
    provider.kind = TextureSourceProviderKind::kAuto;
    provider.displayName = QObject::tr("Auto: MO2 winners");
    provider.coveredTextureCount = a_referenceCount;
    provider.totalTextureCount = a_referenceCount;
    return provider;
}

void AddModCoverage(
    QMap<QString, TextureProviderBuilder>& a_builders,
    QStringList& a_modOrder,
    const MOBase::IModList* a_modList,
    const QString& a_modName,
    const QString& a_textureKey
) {
    const auto* const mod = a_modList->getMod(a_modName);
    if (!mod) {
        return;
    }

    auto& builder = a_builders[a_modName];
    if (builder.sourceName.isEmpty()) {
        a_modOrder.append(a_modName);
        builder.sourceName = a_modName;
        builder.sourcePath = QDir::fromNativeSeparators(mod->absolutePath());
        builder.displayName = a_modList->displayName(a_modName);
        builder.archivePaths = MoDataPaths::ArchivePathsFromMod(mod);
    }
    AppendUnique(builder.coveredTextureKeys, a_textureKey);
}

void AppendModProviders(TextureSourceSet& a_sourceSet, const MOBase::IOrganizer* a_organizer) {
    auto* const modList = a_organizer->modList();
    if (!modList) {
        return;
    }

    QMap<QString, TextureProviderBuilder> modBuilders;
    QStringList modOrder;
    for (const auto& reference : a_sourceSet.references) {
        for (const auto& modName : a_organizer->getFileOrigins(reference.path)) {
            AddModCoverage(modBuilders, modOrder, modList, modName, reference.key);
        }
    }

    OrderModsByProfilePriority(modOrder, modList);
    for (const auto& modName : modOrder) {
        const auto& builder = modBuilders[modName];
        if (!builder.coveredTextureKeys.isEmpty()) {
            a_sourceSet.providers.push_back(
                MakeProvider(TextureSourceProviderKind::kMod, builder, a_sourceSet.references.size())
            );
        }
    }
}

void AppendGameDataProvider(TextureSourceSet& a_sourceSet, const MOBase::IOrganizer* a_organizer) {
    TextureProviderBuilder gameBuilder;
    gameBuilder.displayName = QObject::tr("Game Data");
    if (const auto* const game = a_organizer->managedGame()) {
        gameBuilder.sourcePath = QDir::fromNativeSeparators(game->dataDirectory().absolutePath());
    }
    gameBuilder.archivePaths = MoDataPaths::ArchivePathsFromGame(a_organizer);
    for (const auto& reference : a_sourceSet.references) {
        if (GameDataContainsTexture(a_organizer, reference.path)) {
            AppendUnique(gameBuilder.coveredTextureKeys, reference.key);
        }
    }
    AppendArchiveCoverage(gameBuilder.coveredTextureKeys, gameBuilder.archivePaths, a_sourceSet.references);

    if (!gameBuilder.coveredTextureKeys.isEmpty()) {
        a_sourceSet.providers.push_back(
            MakeProvider(TextureSourceProviderKind::kGameData, gameBuilder, a_sourceSet.references.size())
        );
    }
}
} // namespace

TextureSourceSet TextureSourceResolver::Resolve(MOBase::IOrganizer* a_organizer, const nifly::NifFile* a_nifFile) {
    TextureSourceSet sourceSet;
    sourceSet.references = TextureReferencesFor(a_organizer, a_nifFile);
    sourceSet.providers.push_back(MakeAutoProvider(sourceSet.references.size()));

    if (!a_organizer || sourceSet.references.isEmpty()) {
        return sourceSet;
    }

    AppendModProviders(sourceSet, a_organizer);
    AppendGameDataProvider(sourceSet, a_organizer);
    return sourceSet;
}

QString MakeTextureSummaryText(const TextureSourceSet& a_sourceSet) {
    if (a_sourceSet.references.isEmpty()) {
        return QObject::tr("Textures: none");
    }

    const auto summaries = TextureSlotSummaries(a_sourceSet);

    QString summary = TextureSlotSummaryList(summaries.mid(0, 3));
    if (summaries.size() > 3) {
        summary = QObject::tr("%1 +%2").arg(summary).arg(summaries.size() - 3);
    }

    return QObject::tr("Textures: %1").arg(summary);
}

QString MakeTextureToolTipText(const TextureSourceSet& a_sourceSet, const int a_providerIndex) {
    if (a_sourceSet.references.isEmpty()) {
        return QObject::tr("No referenced textures");
    }

    QStringList lines;
    if (a_providerIndex >= 0 && a_providerIndex < a_sourceSet.providers.size()) {
        const auto& provider = a_sourceSet.providers[a_providerIndex];
        lines << QObject::tr("Texture source: %1").arg(provider.displayName);
    }
    lines << QObject::tr("Texture slots: %1").arg(TextureSlotSummaryList(TextureSlotSummaries(a_sourceSet)));
    lines << QObject::tr("Referenced textures:");
    for (const auto& reference : a_sourceSet.references) {
        lines << QObject::tr("%1: %2").arg(reference.slotName, reference.path);
    }

    return lines.join('\n');
}
