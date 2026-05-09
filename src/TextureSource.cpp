#include "TextureSource.h"
#include "NifExtensions.h"
#include "TextureSlots.h"

#include <NifFile.hpp>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QObject>
#include <QSet>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <ranges>
#include <uibase/game_features/dataarchives.h>
#include <uibase/game_features/igamefeatures.h>
#include <uibase/ifiletree.h>
#include <uibase/imodinterface.h>
#include <uibase/imodlist.h>
#include <uibase/imoinfo.h>
#include <uibase/iplugingame.h>
#include <utility>

#include <libbsarch/bs_archive.h>

namespace
{
struct TextureProviderBuilder
{
  QString sourceName;
  QString sourcePath;
  QString displayName;
  QStringList archivePaths;
  QStringList coveredTextureKeys;
};

struct TextureSlotSummary
{
  QString name;
  int count = 0;
};

const MOBase::IProfile* profilePointer(const MOBase::IProfile* profile)
{
  return profile;
}

template <class Profile>
const MOBase::IProfile* profilePointer(const std::shared_ptr<Profile>& profile)
{
  return profile.get();
}

const MOBase::IProfile* currentProfile(MOBase::IOrganizer* organizer)
{
  if (!organizer) {
    return nullptr;
  }

  return profilePointer(organizer->profile());
}

bool isArchiveName(const QString& name)
{
  return name.endsWith(".bsa", Qt::CaseInsensitive) ||
         name.endsWith(".ba2", Qt::CaseInsensitive);
}

QString resolveDataPath(MOBase::IOrganizer* organizer, const QString& path)
{
  if (!organizer) {
    return {};
  }

  if (auto resolved = organizer->resolvePath(path); !resolved.isEmpty()) {
    return QDir::fromNativeSeparators(QFileInfo(resolved).absoluteFilePath());
  }

  const auto game = organizer->managedGame();
  if (!game) {
    return {};
  }

  const auto dataPath = game->dataDirectory().absoluteFilePath(QDir::cleanPath(path));
  return QFileInfo::exists(dataPath)
             ? QDir::fromNativeSeparators(QFileInfo(dataPath).absoluteFilePath())
             : QString();
}

void appendUnique(QStringList& values, const QString& value)
{
  if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) {
    values.append(value);
  }
}

QStringList archivePathsFromMod(MOBase::IModInterface* mod)
{
  QStringList archivePaths;
  if (!mod) {
    return archivePaths;
  }

  const auto fileTree = mod->fileTree();
  if (!fileTree) {
    return archivePaths;
  }

  for (auto it = fileTree->begin(); it != fileTree->end(); ++it) {
    const auto fileInfo = *it;
    if (!fileInfo || !fileInfo->isFile() || !isArchiveName(fileInfo->name())) {
      continue;
    }

    const QFileInfo archiveInfo(QDir(mod->absolutePath()).filePath(fileInfo->name()));
    if (archiveInfo.exists() && archiveInfo.isFile()) {
      appendUnique(archivePaths,
                   QDir::fromNativeSeparators(archiveInfo.absoluteFilePath()));
    }
  }

  return archivePaths;
}

QStringList archivePathsFromGame(MOBase::IOrganizer* organizer)
{
  QStringList archivePaths;
  if (!organizer) {
    return archivePaths;
  }

  const auto features = organizer->gameFeatures();
  if (!features) {
    return archivePaths;
  }

  const auto gameArchives = features->gameFeature<MOBase::DataArchives>();
  if (!gameArchives) {
    return archivePaths;
  }

  for (auto archives = gameArchives->archives(currentProfile(organizer));
       const auto& archive : std::ranges::reverse_view(archives)) {
    appendUnique(archivePaths, resolveDataPath(organizer, archive));
  }

  return archivePaths;
}

bool loadArchivePath(libbsarch::bs_archive& archive, const QString& archivePath)
{
  try {
    archive.load_from_disk(QDir::toNativeSeparators(archivePath).toStdWString());
    return true;
  } catch (const std::exception& exception) {
    qWarning("Failed to load BSA archive '%s' while resolving texture sources: %s",
             qUtf8Printable(archivePath), exception.what());
    return false;
  }
}

bool archiveContainsTexture(libbsarch::bs_archive& archive, const QString& texturePath)
{
  const auto dataPath   = normalizeTextureDataPath(texturePath);
  auto paths            = QStringList{dataPath};
  const auto nativePath = QDir::toNativeSeparators(dataPath);
  appendUnique(paths, nativePath);

  for (const auto& path : paths) {
    try {
      if (archive.find_file_record(path.toStdWString())) {
        return true;
      }
    } catch (const std::exception&) {
    }
  }

  return false;
}

void appendArchiveCoverage(QStringList& coveredTextureKeys,
                           const QStringList& archivePaths,
                           const QVector<TextureReference>& references)
{
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

bool gameDataContainsTexture(MOBase::IOrganizer* organizer, const QString& texturePath)
{
  if (!organizer) {
    return false;
  }

  const auto game = organizer->managedGame();
  if (!game) {
    return false;
  }

  const auto dataPath =
      game->dataDirectory().absoluteFilePath(QDir::cleanPath(texturePath));
  return QFileInfo::exists(dataPath) && QFileInfo(dataPath).isFile();
}

QString textureSlotName(const nifly::NiShader* shader, const std::size_t slot)
{
  switch (slot) {
  case TextureSlot::BaseMap:
    return QObject::tr("Base");
  case TextureSlot::NormalMap:
    return QObject::tr("Normal");
  case TextureSlot::GlowMap:
    return shader && shader->HasGlowmap() ? QObject::tr("Glow") : QObject::tr("Light");
  case TextureSlot::HeightMap:
    if (const auto bslsp =
            dynamic_cast<const nifly::BSLightingShaderProperty*>(shader)) {
      const auto shaderType = bslsp->GetShaderType();
      if (shaderType == nifly::BSLSP_PARALLAX ||
          shaderType == nifly::BSLSP_PARALLAXOCC ||
          shaderType == nifly::BSLSP_MULTILAYERPARALLAX) {
        return QObject::tr("Height");
      }
    }
    return QObject::tr("Detail");
  case TextureSlot::EnvironmentMap:
    return QObject::tr("Environment");
  case TextureSlot::EnvironmentMask:
    return QObject::tr("Env Mask");
  case TextureSlot::TintMask:
    if (const auto bslsp = dynamic_cast<const nifly::BSLightingShaderProperty*>(shader);
        bslsp && bslsp->GetShaderType() == nifly::BSLSP_MULTILAYERPARALLAX) {
      return QObject::tr("Inner");
    }
    return QObject::tr("Tint");
  case TextureSlot::BacklightMap:
    return shader && shader->HasBacklight() ? QObject::tr("Backlight")
                                            : QObject::tr("Specular");
  default:
    return QObject::tr("Slot %1").arg(slot + 1);
  }
}

QVector<TextureReference> textureReferencesFor(const nifly::NifFile* nifFile)
{
  QVector<TextureReference> references;
  QSet<QString> seenPaths;
  if (!nifFile) {
    return references;
  }

  for (auto* shape : nifFile->GetShapes()) {
    if (!shape || shape->flags & TriShape::Hidden) {
      continue;
    }

    const auto shader = nifFile->GetShader(shape);
    if (!shader || !shader->HasTextureSet()) {
      continue;
    }

    const auto textureSet = nifFile->GetHeader().GetBlock(shader->TextureSetRef());
    if (!textureSet) {
      continue;
    }

    const auto textureCount =
        std::min<std::size_t>(textureSet->textures.size(), TextureSlotCount);
    for (std::size_t i = 0; i < textureCount; i++) {
      const auto path = normalizeTextureDataPath(QString::fromStdString(
          textureSet->textures[static_cast<std::uint32_t>(i)].get()));
      const auto key  = textureDataPathKey(path);
      if (path.isEmpty() || seenPaths.contains(key)) {
        continue;
      }

      seenPaths.insert(key);
      references.push_back(
          {static_cast<int>(i), textureSlotName(shader, i), path, key});
    }
  }

  return references;
}

QString coverageDisplayName(const QString& name, const int covered, const int total)
{
  return QObject::tr("%1 (%2/%3)").arg(name).arg(covered).arg(total);
}

QVector<TextureSlotSummary> textureSlotSummaries(const TextureSourceSet& sourceSet)
{
  QVector<TextureSlotSummary> summaries;
  for (const auto& reference : sourceSet.references) {
    const auto existing = std::ranges::find_if(summaries, [&](const auto& summary) {
      return summary.name == reference.slotName;
    });
    if (existing != summaries.end()) {
      existing->count++;
      continue;
    }

    summaries.push_back({reference.slotName, 1});
  }

  return summaries;
}

QString textureSlotSummaryLabel(const TextureSlotSummary& summary)
{
  return QObject::tr("%1 x%2").arg(summary.name).arg(summary.count);
}

QString textureSlotSummaryList(const QVector<TextureSlotSummary>& summaries)
{
  QStringList labels;
  for (const auto& summary : summaries) {
    labels << textureSlotSummaryLabel(summary);
  }

  return labels.join(QObject::tr(", "));
}

TextureSourceProvider makeProvider(TextureSourceProviderKind kind,
                                   const TextureProviderBuilder& builder,
                                   const int totalTextureCount)
{
  TextureSourceProvider provider;
  provider.kind                = kind;
  provider.sourceName          = builder.sourceName;
  provider.sourcePath          = builder.sourcePath;
  provider.archivePaths        = builder.archivePaths;
  provider.coveredTextureKeys  = builder.coveredTextureKeys;
  provider.coveredTextureCount = builder.coveredTextureKeys.size();
  provider.totalTextureCount   = totalTextureCount;
  provider.displayName         = coverageDisplayName(
      builder.displayName, provider.coveredTextureCount, provider.totalTextureCount);
  return provider;
}
}  // namespace

QString normalizeTextureDataPath(QString path)
{
  path = QDir::fromNativeSeparators(path).trimmed();
  while (path.startsWith('/')) {
    path.remove(0, 1);
  }
  return path;
}

QString textureDataPathKey(const QString& path)
{
  return normalizeTextureDataPath(path).toLower();
}

bool textureProviderCoversPath(const TextureSourceProvider& provider,
                               const QString& texturePath)
{
  return provider.coveredTextureKeys.contains(textureDataPathKey(texturePath),
                                              Qt::CaseInsensitive);
}

TextureSourceSet TextureSourceResolver::resolve(MOBase::IOrganizer* organizer,
                                                const nifly::NifFile* nifFile)
{
  TextureSourceSet sourceSet;
  sourceSet.references = textureReferencesFor(nifFile);

  TextureSourceProvider autoProvider;
  autoProvider.kind                = TextureSourceProviderKind::Auto;
  autoProvider.displayName         = QObject::tr("Auto: MO2 winners");
  autoProvider.coveredTextureCount = sourceSet.references.size();
  autoProvider.totalTextureCount   = sourceSet.references.size();
  sourceSet.providers.push_back(std::move(autoProvider));

  if (!organizer || sourceSet.references.isEmpty()) {
    return sourceSet;
  }

  const auto modList = organizer->modList();
  QMap<QString, TextureProviderBuilder> modBuilders;
  QStringList modOrder;
  if (modList) {
    for (const auto& reference : sourceSet.references) {
      const auto origins = organizer->getFileOrigins(reference.path);
      for (const auto& modName : origins) {
        const auto mod = modList->getMod(modName);
        if (!mod) {
          continue;
        }

        auto& builder = modBuilders[modName];
        if (builder.sourceName.isEmpty()) {
          modOrder.append(modName);
          builder.sourceName   = modName;
          builder.sourcePath   = QDir::fromNativeSeparators(mod->absolutePath());
          builder.displayName  = modList->displayName(modName);
          builder.archivePaths = archivePathsFromMod(mod);
        }
        appendUnique(builder.coveredTextureKeys, reference.key);
      }
    }
  }

  for (const auto& modName : modOrder) {
    const auto& builder = modBuilders[modName];
    if (!builder.coveredTextureKeys.isEmpty()) {
      sourceSet.providers.push_back(makeProvider(TextureSourceProviderKind::Mod,
                                                 builder, sourceSet.references.size()));
    }
  }

  TextureProviderBuilder gameBuilder;
  gameBuilder.displayName = QObject::tr("Game Data");
  gameBuilder.sourcePath =
      organizer->managedGame()
          ? QDir::fromNativeSeparators(
                organizer->managedGame()->dataDirectory().absolutePath())
          : QString();
  gameBuilder.archivePaths = archivePathsFromGame(organizer);
  for (const auto& reference : sourceSet.references) {
    if (gameDataContainsTexture(organizer, reference.path)) {
      appendUnique(gameBuilder.coveredTextureKeys, reference.key);
    }
  }
  appendArchiveCoverage(gameBuilder.coveredTextureKeys, gameBuilder.archivePaths,
                        sourceSet.references);

  if (!gameBuilder.coveredTextureKeys.isEmpty()) {
    sourceSet.providers.push_back(makeProvider(
        TextureSourceProviderKind::GameData, gameBuilder, sourceSet.references.size()));
  }

  return sourceSet;
}

QString makeTextureSummaryText(const TextureSourceSet& sourceSet)
{
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

QString makeTextureToolTipText(const TextureSourceSet& sourceSet,
                               const int providerIndex)
{
  if (sourceSet.references.isEmpty()) {
    return QObject::tr("No referenced textures");
  }

  QStringList lines;
  if (providerIndex >= 0 && providerIndex < sourceSet.providers.size()) {
    const auto& provider = sourceSet.providers[providerIndex];
    lines << QObject::tr("Texture source: %1").arg(provider.displayName);
  }
  lines << QObject::tr("Texture slots: %1")
               .arg(textureSlotSummaryList(textureSlotSummaries(sourceSet)));
  lines << QObject::tr("Referenced textures:");
  for (const auto& reference : sourceSet.references) {
    lines << QObject::tr("%1: %2").arg(reference.slotName, reference.path);
  }

  return lines.join('\n');
}
