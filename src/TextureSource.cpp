#include "TextureSource.h"
#include "ArchiveAccess.h"
#include "MoDataPaths.h"
#include "NifExtensions.h"
#include "ShaderClassification.h"
#include "TextureSlotDescriptors.h"

#include <NifFile.hpp>

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

void appendUnique(QStringList& values, const QString& value) {
    if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) {
        values.append(value);
    }
}

bool loadArchivePath(libbsarch::bs_archive& archive, const QString& archivePath) {
    QString error;
    if (ArchiveAccess::loadArchive(archive, archivePath, &error)) {
        return true;
    }

    qWarning(
        "Failed to load BSA archive '%s' while resolving texture sources: %s",
        qUtf8Printable(archivePath),
        qUtf8Printable(error)
    );
    return false;
}

bool archiveContainsTexture(libbsarch::bs_archive& archive, const QString& texturePath) {
    QString errorPath;
    QString error;
    if (ArchiveAccess::containsDataPath(archive, texturePath, &errorPath, &error)) {
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

void appendArchiveCoverage(
    QStringList& coveredTextureKeys,
    const QStringList& archivePaths,
    const QVector<TextureReference>& references
) {
    for (const auto& archivePath : archivePaths) {
        if (coveredTextureKeys.size() >= references.size()) {
            return;
        }

        libbsarch::bs_archive archive;
        if (archivePath.isEmpty() || !loadArchivePath(archive, archivePath)) {
            continue;
        }

        for (const auto& reference : references) {
            if (coveredTextureKeys.contains(reference.key, Qt::CaseInsensitive)) {
                continue;
            }

            if (archiveContainsTexture(archive, reference.path)) {
                appendUnique(coveredTextureKeys, reference.key);
            }
        }
    }
}

bool gameDataContainsTexture(MOBase::IOrganizer* organizer, const QString& texturePath) {
    if (!organizer) {
        return false;
    }

    const auto* const game = organizer->managedGame();
    if (!game) {
        return false;
    }

    const auto dataPath = game->dataDirectory().absoluteFilePath(QDir::cleanPath(texturePath));
    return QFileInfo::exists(dataPath) && QFileInfo(dataPath).isFile();
}

void appendTextureReference(
    QVector<TextureReference>& references,
    QSet<QString>& seenPaths,
    const nifly::NiShader* shader,
    const ShaderManager::ShaderType shaderType,
    const int slot,
    const QString& texturePath,
    const bool isRefractionProxy = false
) {
    const auto path = normalizeTextureDataPath(texturePath);
    const auto key = textureDataPathKey(path);
    if (path.isEmpty()) {
        return;
    }

    const auto slotName = textureSlotDisplayName(shader, shaderType, static_cast<std::size_t>(slot), isRefractionProxy);

    if (seenPaths.contains(key)) {
        if (isRefractionProxy && slot == TextureSlot::BaseMap) {
            const auto it = std::ranges::find_if(references, [&](const auto& reference) {
                return reference.key == key;
            });
            if (it != references.end()) {
                it->slot = slot;
                it->slotName = slotName;
            }
        }
        return;
    }

    seenPaths.insert(key);
    references.push_back({.slot = slot, .slotName = slotName, .path = path, .key = key});
}

void appendEffectShaderTextureReferences(
    QVector<TextureReference>& references,
    QSet<QString>& seenPaths,
    const nifly::BSEffectShaderProperty* effectShader,
    const ShaderManager::ShaderType shaderType
) {
    appendTextureReference(
        references,
        seenPaths,
        effectShader,
        shaderType,
        TextureSlot::BaseMap,
        QString::fromStdString(effectShader->sourceTexture.get())
    );
    appendTextureReference(
        references,
        seenPaths,
        effectShader,
        shaderType,
        TextureSlot::GreyscaleMap,
        QString::fromStdString(effectShader->greyscaleTexture.get())
    );

    if (shaderType != ShaderManager::FO4EffectShader) {
        return;
    }

    appendTextureReference(
        references,
        seenPaths,
        effectShader,
        shaderType,
        TextureSlot::NormalMap,
        QString::fromStdString(effectShader->normalTexture.get())
    );
    appendTextureReference(
        references,
        seenPaths,
        effectShader,
        shaderType,
        TextureSlot::EnvironmentMap,
        QString::fromStdString(effectShader->envMapTexture.get())
    );
    appendTextureReference(
        references,
        seenPaths,
        effectShader,
        shaderType,
        TextureSlot::EnvironmentMask,
        QString::fromStdString(effectShader->envMaskTexture.get())
    );
}

QVector<TextureReference> textureReferencesFor(const nifly::NifFile* nifFile) {
    QVector<TextureReference> references;
    QSet<QString> seenPaths;
    if (!nifFile) {
        return references;
    }

    for (auto* shape : nifFile->GetShapes()) {
        if (!shape || shape->flags & TriShape::Hidden) {
            continue;
        }

        auto* const shader = nifFile->GetShader(shape);
        if (!shader) {
            continue;
        }

        const auto isRefractionProxy = IsRefractionDistortionProxy(nifFile, shape);
        const auto shaderType = classifyShaderType(nifFile, shader);

        if (auto* const effectShader = dynamic_cast<nifly::BSEffectShaderProperty*>(shader)) {
            appendEffectShaderTextureReferences(references, seenPaths, effectShader, shaderType);
        }

        if (!shader->HasTextureSet()) {
            continue;
        }

        auto* const textureSet = nifFile->GetHeader().GetBlock(shader->TextureSetRef());
        if (!textureSet) {
            continue;
        }

        const auto textureCount = std::min<std::size_t>(textureSet->textures.size(), TextureSlotCount);
        for (std::size_t i = 0; i < textureCount; i++) {
            appendTextureReference(
                references,
                seenPaths,
                shader,
                shaderType,
                static_cast<int>(i),
                QString::fromStdString(textureSet->textures[static_cast<std::uint32_t>(i)].get()),
                isRefractionProxy
            );
        }
    }

    return references;
}

QString coverageDisplayName(const QString& name, const qsizetype covered, const qsizetype total) {
    return QObject::tr("%1 (%2/%3)").arg(name).arg(covered).arg(total);
}

QVector<TextureSlotSummary> textureSlotSummaries(const TextureSourceSet& sourceSet) {
    QVector<TextureSlotSummary> summaries;
    for (const auto& reference : sourceSet.references) {
        const auto existing = std::ranges::find_if(summaries, [&](const auto& summary) {
            return summary.name == reference.slotName;
        });
        if (existing != summaries.end()) {
            existing->count++;
            continue;
        }

        summaries.push_back({.name = reference.slotName, .count = 1});
    }

    return summaries;
}

QString textureSlotSummaryLabel(const TextureSlotSummary& summary) {
    if (summary.count <= 1) {
        return summary.name;
    }

    return QObject::tr("%1 x%2").arg(summary.name).arg(summary.count);
}

QString textureSlotSummaryList(const QVector<TextureSlotSummary>& summaries) {
    QStringList labels;
    for (const auto& summary : summaries) {
        labels << textureSlotSummaryLabel(summary);
    }

    return labels.join(QObject::tr(", "));
}

TextureSourceProvider makeProvider(
    TextureSourceProviderKind kind,
    const TextureProviderBuilder& builder,
    const qsizetype totalTextureCount
) {
    TextureSourceProvider provider;
    provider.kind = kind;
    provider.sourceName = builder.sourceName;
    provider.sourcePath = builder.sourcePath;
    provider.archivePaths = builder.archivePaths;
    provider.coveredTextureKeys = builder.coveredTextureKeys;
    provider.coveredTextureCount = builder.coveredTextureKeys.size();
    provider.totalTextureCount = totalTextureCount;
    provider.displayName = coverageDisplayName(
        builder.displayName,
        provider.coveredTextureCount,
        provider.totalTextureCount
    );
    return provider;
}
} // namespace

QString normalizeTextureDataPath(QString path) {
    path = QDir::fromNativeSeparators(path).trimmed();
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }
    return path;
}

QString textureDataPathKey(const QString& path) {
    return normalizeTextureDataPath(path).toLower();
}

bool textureProviderCoversPath(const TextureSourceProvider& provider, const QString& texturePath) {
    return provider.coveredTextureKeys.contains(textureDataPathKey(texturePath), Qt::CaseInsensitive);
}

TextureSourceSet TextureSourceResolver::resolve(MOBase::IOrganizer* organizer, const nifly::NifFile* nifFile) {
    TextureSourceSet sourceSet;
    sourceSet.references = textureReferencesFor(nifFile);

    TextureSourceProvider autoProvider;
    autoProvider.kind = TextureSourceProviderKind::Auto;
    autoProvider.displayName = QObject::tr("Auto: MO2 winners");
    autoProvider.coveredTextureCount = sourceSet.references.size();
    autoProvider.totalTextureCount = sourceSet.references.size();
    sourceSet.providers.push_back(std::move(autoProvider));

    if (!organizer || sourceSet.references.isEmpty()) {
        return sourceSet;
    }

    auto* const modList = organizer->modList();
    QMap<QString, TextureProviderBuilder> modBuilders;
    QStringList modOrder;
    if (modList) {
        for (const auto& reference : sourceSet.references) {
            const auto origins = organizer->getFileOrigins(reference.path);
            for (const auto& modName : origins) {
                auto* const mod = modList->getMod(modName);
                if (!mod) {
                    continue;
                }

                auto& builder = modBuilders[modName];
                if (builder.sourceName.isEmpty()) {
                    modOrder.append(modName);
                    builder.sourceName = modName;
                    builder.sourcePath = QDir::fromNativeSeparators(mod->absolutePath());
                    builder.displayName = modList->displayName(modName);
                    builder.archivePaths = MoDataPaths::archivePathsFromMod(mod);
                }
                appendUnique(builder.coveredTextureKeys, reference.key);
            }
        }
    }

    for (const auto& modName : modOrder) {
        const auto& builder = modBuilders[modName];
        if (!builder.coveredTextureKeys.isEmpty()) {
            sourceSet.providers.push_back(
                makeProvider(TextureSourceProviderKind::Mod, builder, sourceSet.references.size())
            );
        }
    }

    TextureProviderBuilder gameBuilder;
    gameBuilder.displayName = QObject::tr("Game Data");
    gameBuilder.sourcePath = organizer->managedGame()
                                 ? QDir::fromNativeSeparators(organizer->managedGame()->dataDirectory().absolutePath())
                                 : QString();
    gameBuilder.archivePaths = MoDataPaths::archivePathsFromGame(organizer);
    for (const auto& reference : sourceSet.references) {
        if (gameDataContainsTexture(organizer, reference.path)) {
            appendUnique(gameBuilder.coveredTextureKeys, reference.key);
        }
    }
    appendArchiveCoverage(gameBuilder.coveredTextureKeys, gameBuilder.archivePaths, sourceSet.references);

    if (!gameBuilder.coveredTextureKeys.isEmpty()) {
        sourceSet.providers.push_back(
            makeProvider(TextureSourceProviderKind::GameData, gameBuilder, sourceSet.references.size())
        );
    }

    return sourceSet;
}

QString makeTextureSummaryText(const TextureSourceSet& sourceSet) {
    if (sourceSet.references.isEmpty()) {
        return QObject::tr("Textures: none");
    }

    const auto summaries = textureSlotSummaries(sourceSet);

    QString summary = textureSlotSummaryList(summaries.mid(0, 3));
    if (summaries.size() > 3) {
        summary = QObject::tr("%1 +%2").arg(summary).arg(summaries.size() - 3);
    }

    return QObject::tr("Textures: %1").arg(summary);
}

QString makeTextureToolTipText(const TextureSourceSet& sourceSet, const int providerIndex) {
    if (sourceSet.references.isEmpty()) {
        return QObject::tr("No referenced textures");
    }

    QStringList lines;
    if (providerIndex >= 0 && providerIndex < sourceSet.providers.size()) {
        const auto& provider = sourceSet.providers[providerIndex];
        lines << QObject::tr("Texture source: %1").arg(provider.displayName);
    }
    lines << QObject::tr("Texture slots: %1").arg(textureSlotSummaryList(textureSlotSummaries(sourceSet)));
    lines << QObject::tr("Referenced textures:");
    for (const auto& reference : sourceSet.references) {
        lines << QObject::tr("%1: %2").arg(reference.slotName, reference.path);
    }

    return lines.join('\n');
}
