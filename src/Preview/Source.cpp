#include "Preview/Source.h"
#include "Data/ArchiveAccess.h"
#include "Data/MoDataPaths.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QStringList>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <ranges>
#include <sstream>
#include <uibase/imodinterface.h>
#include <uibase/imodlist.h>
#include <uibase/imoinfo.h>
#include <uibase/iplugingame.h>
#include <utility>

#include <libbsarch/bs_archive.h>

namespace {
QString NormalizeDataPath(QString a_path) {
    a_path = QDir::fromNativeSeparators(a_path).trimmed();
    while (a_path.startsWith('/')) {
        a_path.remove(0, 1);
    }
    return a_path;
}

QString RelativeToBase(const QString& a_basePath, const QString& a_fileName) {
    if (a_basePath.isEmpty()) {
        return {};
    }

    const auto relativePath = QDir(QDir::fromNativeSeparators(a_basePath)).relativeFilePath(a_fileName);
    if (relativePath.isEmpty() || relativePath.startsWith("../") || QDir::isAbsolutePath(relativePath)) {
        return {};
    }

    return NormalizeDataPath(relativePath);
}

QString FallbackMeshesPath(const QString& a_fileName) {
    const auto normalized = QDir::fromNativeSeparators(a_fileName);
    const auto lower = normalized.toLower();

    if (lower.startsWith("meshes/")) {
        return NormalizeDataPath(normalized);
    }

    const auto marker = QStringLiteral("/meshes/");
    const auto index = lower.indexOf(marker);
    if (index < 0) {
        return {};
    }

    return NormalizeDataPath(normalized.mid(index + 1));
}

QString RelativeToGameData(const MOBase::IOrganizer* a_organizer, const QString& a_path) {
    const auto* const game = a_organizer->managedGame();
    return game ? RelativeToBase(game->dataDirectory().absolutePath(), a_path) : QString();
}

QString RelativeToMods(const MOBase::IOrganizer* a_organizer, const QString& a_path) {
    const auto* const modList = a_organizer->modList();
    if (!modList) {
        return {};
    }

    for (const auto& modName : modList->allModsByProfilePriority()) {
        const auto* const mod = modList->getMod(modName);
        if (!mod) {
            continue;
        }

        if (auto relativePath = RelativeToBase(mod->absolutePath(), a_path); !relativePath.isEmpty()) {
            return relativePath;
        }
    }

    return {};
}

QString VirtualPathFor(const MOBase::IOrganizer* a_organizer, const QString& a_fileName) {
    const auto normalized = QDir::fromNativeSeparators(a_fileName);
    if (!QDir::isAbsolutePath(normalized)) {
        return NormalizeDataPath(normalized);
    }

    if (!a_organizer) {
        return FallbackMeshesPath(normalized);
    }

    if (auto relativePath = RelativeToGameData(a_organizer, normalized); !relativePath.isEmpty()) {
        return relativePath;
    }

    if (auto relativePath = RelativeToMods(a_organizer, normalized); !relativePath.isEmpty()) {
        return relativePath;
    }

    if (auto relativePath = RelativeToBase(a_organizer->overwritePath(), normalized); !relativePath.isEmpty()) {
        return relativePath;
    }

    return FallbackMeshesPath(normalized);
}

bool SamePath(const QString& a_lhs, const QString& a_rhs) {
    return QFileInfo(a_lhs).absoluteFilePath().compare(QFileInfo(a_rhs).absoluteFilePath(), Qt::CaseInsensitive) == 0;
}

bool SameDataPath(const QString& a_lhs, const QString& a_rhs) {
    return NormalizeDataPath(a_lhs).compare(NormalizeDataPath(a_rhs), Qt::CaseInsensitive) == 0;
}

bool HasProviderPath(const QVector<NifPreviewProvider>& a_providers, const QString& a_path) {
    return std::ranges::any_of(a_providers, [&](const NifPreviewProvider& a_provider) {
        return a_provider.kind == NifPreviewProviderKind::kLooseFile
               && !a_provider.absolutePath.isEmpty()
               && SamePath(a_provider.absolutePath, a_path);
    });
}

bool HasArchiveProvider(
    const QVector<NifPreviewProvider>& a_providers,
    const QString& a_archivePath,
    const QString& a_virtualPath
) {
    return std::ranges::any_of(a_providers, [&](const NifPreviewProvider& a_provider) {
        return a_provider.kind == NifPreviewProviderKind::kArchive
               && !a_provider.archivePath.isEmpty()
               && SamePath(a_provider.archivePath, a_archivePath)
               && SameDataPath(a_provider.virtualPath, a_virtualPath);
    });
}

QByteArray ExtractArchiveFile(const QString& a_archivePath, const QString& a_virtualPath) {
    libbsarch::bs_archive archive;

    QString error;
    if (!ArchiveAccess::LoadArchive(archive, a_archivePath, &error)) {
        qWarning("Failed to load BSA archive '%s': %s", qUtf8Printable(a_archivePath), qUtf8Printable(error));
        return {};
    }

    ArchiveAccess::ExtractResult result;
    auto data = ArchiveAccess::ExtractBytes(archive, a_virtualPath, std::numeric_limits<int>::max(), &result);
    if (result.status == ArchiveAccess::ExtractStatus::kOversized) {
        qWarning(
            "Skipping oversized NIF '%s' from BSA '%s'",
            qUtf8Printable(a_virtualPath),
            qUtf8Printable(a_archivePath)
        );
    }

    return data;
}

QString ProviderDisplayName(const QString& a_sourceName, const QString& a_archivePath) {
    auto archiveName = QFileInfo(a_archivePath).fileName();
    if (a_sourceName.isEmpty()) {
        return archiveName;
    }

    return QObject::tr("%1 - %2").arg(a_sourceName, archiveName);
}

void AddLooseProvider(
    QVector<NifPreviewProvider>& a_providers,
    const QString& a_displayName,
    const QString& a_virtualPath,
    const QString& a_absolutePath
) {
    if (a_absolutePath.isEmpty()
        || !QFileInfo::exists(a_absolutePath)
        || HasProviderPath(a_providers, a_absolutePath)) {
        return;
    }

    a_providers.push_back({
        .displayName = a_displayName,
        .virtualPath = a_virtualPath,
        .kind = NifPreviewProviderKind::kLooseFile,
        .absolutePath = QDir::fromNativeSeparators(QFileInfo(a_absolutePath).absoluteFilePath()),
        .archivePath = {},
        .archiveName = {},
        .data = {},
    });
}

void AddArchiveProvider(
    QVector<NifPreviewProvider>& a_providers,
    const QString& a_sourceName,
    const QString& a_virtualPath,
    const QString& a_archivePath
) {
    const QFileInfo archiveInfo(a_archivePath);
    if (a_archivePath.isEmpty() || !archiveInfo.exists() || !archiveInfo.isFile()) {
        return;
    }

    const auto absoluteArchivePath = QDir::fromNativeSeparators(archiveInfo.absoluteFilePath());
    if (HasArchiveProvider(a_providers, absoluteArchivePath, a_virtualPath)) {
        return;
    }

    auto archiveData = ExtractArchiveFile(absoluteArchivePath, a_virtualPath);
    if (archiveData.isEmpty()) {
        return;
    }

    a_providers.push_back({
        .displayName = ProviderDisplayName(a_sourceName, absoluteArchivePath),
        .virtualPath = a_virtualPath,
        .kind = NifPreviewProviderKind::kArchive,
        .absolutePath = {},
        .archivePath = absoluteArchivePath,
        .archiveName = archiveInfo.fileName(),
        .data = std::move(archiveData),
    });
}

void AddArchiveProvidersFromMod(
    QVector<NifPreviewProvider>& a_providers,
    const MOBase::IModInterface* a_mod,
    const QString& a_virtualPath
) {
    if (!a_mod) {
        return;
    }

    for (const auto& archivePath : MoDataPaths::ArchivePathsFromMod(a_mod)) {
        AddArchiveProvider(a_providers, a_mod->name(), a_virtualPath, archivePath);
    }
}

void AddGameArchiveProviders(
    QVector<NifPreviewProvider>& a_providers,
    const MOBase::IOrganizer* a_organizer,
    const QString& a_virtualPath
) {
    if (!a_organizer) {
        return;
    }

    const auto* const game = a_organizer->managedGame();
    const auto sourceName = game ? game->displayGameName() : QObject::tr("Game Data");

    for (const auto& archivePath : MoDataPaths::ArchivePathsFromGame(a_organizer)) {
        AddArchiveProvider(a_providers, sourceName, a_virtualPath, archivePath);
    }
}

void OrderOriginsForPreview(QStringList& a_origins) {
    // MO2 returns the primary winner first, then alternatives in losing-to-winning order.
    // Keep the winner first and present the alternatives from strongest to weakest.
    if (a_origins.size() > 1) {
        std::reverse(a_origins.begin() + 1, a_origins.end());
    }
}

int ProviderIndexForPath(const QVector<NifPreviewProvider>& a_providers, const QString& a_path) {
    if (a_path.isEmpty()) {
        return -1;
    }

    for (int i = 0; i < a_providers.size(); ++i) {
        if (!a_providers[i].absolutePath.isEmpty() && SamePath(a_providers[i].absolutePath, a_path)) {
            return i;
        }
    }

    return -1;
}

int ProviderIndexForArchiveData(const QVector<NifPreviewProvider>& a_providers, const QByteArray& a_fileData) {
    if (a_fileData.isEmpty()) {
        return -1;
    }

    for (int i = 0; i < a_providers.size(); ++i) {
        const auto& provider = a_providers[i];
        if (provider.kind != NifPreviewProviderKind::kArchive) {
            continue;
        }

        const auto archiveData = provider.data.isEmpty()
                                     ? ExtractArchiveFile(provider.archivePath, provider.virtualPath)
                                     : provider.data;
        if (archiveData.size() == a_fileData.size() && archiveData == a_fileData) {
            return i;
        }
    }

    return -1;
}

void AddInMemoryProvider(
    QVector<NifPreviewProvider>& a_providers,
    const QString& a_displayName,
    const QString& a_virtualPath,
    const QString& a_fileName,
    const QByteArray& a_fileData
) {
    a_providers.insert(
        a_providers.begin(),
        {
            .displayName = a_displayName,
            .virtualPath = a_virtualPath,
            .kind = NifPreviewProviderKind::kInMemory,
            .absolutePath = a_fileName,
            .archivePath = {},
            .archiveName = {},
            .data = a_fileData,
        }
    );
}

void AddOrganizerProviders(NifPreviewSourceSet& a_sourceSet, const MOBase::IOrganizer* a_organizer) {
    if (const auto* const modList = a_organizer->modList()) {
        auto origins = a_organizer->getFileOrigins(a_sourceSet.virtualPath);
        OrderOriginsForPreview(origins);
        for (const auto& modName : origins) {
            const auto* const mod = modList->getMod(modName);
            if (!mod) {
                continue;
            }

            const auto absolutePath = QDir(mod->absolutePath()).filePath(a_sourceSet.virtualPath);
            AddLooseProvider(a_sourceSet.providers, mod->name(), a_sourceSet.virtualPath, absolutePath);
            AddArchiveProvidersFromMod(a_sourceSet.providers, mod, a_sourceSet.virtualPath);
        }
    }

    AddGameArchiveProviders(a_sourceSet.providers, a_organizer, a_sourceSet.virtualPath);
}

void SelectInMemoryProvider(NifPreviewSourceSet& a_sourceSet, const QString& a_fileName, const QByteArray& a_fileData) {
    if (const auto currentIndex = ProviderIndexForArchiveData(a_sourceSet.providers, a_fileData); currentIndex >= 0) {
        a_sourceSet.currentIndex = currentIndex;
        return;
    }

    AddInMemoryProvider(
        a_sourceSet.providers,
        QObject::tr("Current Archive Preview"),
        a_sourceSet.virtualPath,
        a_fileName,
        a_fileData
    );
    a_sourceSet.currentIndex = 0;
}

void SelectLooseFileProvider(NifPreviewSourceSet& a_sourceSet, const QString& a_fileName) {
    AddLooseProvider(a_sourceSet.providers, QFileInfo(a_fileName).fileName(), a_sourceSet.virtualPath, a_fileName);
    if (const auto currentIndex = ProviderIndexForPath(a_sourceSet.providers, a_fileName); currentIndex >= 0) {
        a_sourceSet.currentIndex = currentIndex;
    }
}
} // namespace

NifPreviewSourceSet NifPreviewSourceResolver::Resolve(
    MOBase::IOrganizer* a_organizer,
    const QString& a_fileName,
    const QByteArray& a_fileData
) {
    NifPreviewSourceSet sourceSet;
    sourceSet.virtualPath = VirtualPathFor(a_organizer, a_fileName);

    if (a_organizer && !sourceSet.virtualPath.isEmpty()) {
        AddOrganizerProviders(sourceSet, a_organizer);
    }

    const auto normalizedFileName = QDir::fromNativeSeparators(a_fileName);
    if (!a_fileData.isEmpty()) {
        SelectInMemoryProvider(sourceSet, normalizedFileName, a_fileData);
    } else if (QFileInfo::exists(normalizedFileName)) {
        SelectLooseFileProvider(sourceSet, normalizedFileName);
    }

    if (sourceSet.currentIndex < 0 || sourceSet.currentIndex >= sourceSet.providers.size()) {
        sourceSet.currentIndex = 0;
    }

    return sourceSet;
}

std::shared_ptr<nifly::NifFile> LoadNifProvider(const NifPreviewProvider& a_provider) {
    std::shared_ptr<nifly::NifFile> nifFile;

    if (a_provider.kind == NifPreviewProviderKind::kInMemory || a_provider.kind == NifPreviewProviderKind::kArchive) {
        const auto data = a_provider.kind == NifPreviewProviderKind::kArchive && a_provider.data.isEmpty()
                              ? ExtractArchiveFile(a_provider.archivePath, a_provider.virtualPath)
                              : a_provider.data;
        if (data.isEmpty()) {
            return nullptr;
        }

        const std::string bytes(data.constData(), static_cast<std::size_t>(data.size()));
        std::istringstream fileStream(bytes);
        nifFile = std::make_shared<nifly::NifFile>(fileStream);
    } else {
        nifFile = std::make_shared<nifly::NifFile>(std::filesystem::path(a_provider.absolutePath.toStdWString()));
    }

    if (!nifFile || !nifFile->IsValid()) {
        return nullptr;
    }

    return nifFile;
}

QString MakeNifStatsText(const nifly::NifFile* a_nifFile) {
    unsigned int shapes = 0;
    unsigned int faces = 0;
    unsigned int verts = 0;

    if (a_nifFile) {
        for (const auto& shape : a_nifFile->GetShapes()) {
            if (!shape) {
                continue;
            }
            shapes++;
            faces += shape->GetNumTriangles();
            verts += shape->GetNumVertices();
        }
    }

    return QObject::tr("Verts: %1 | Faces: %2 | Shapes: %3").arg(verts).arg(faces).arg(shapes);
}
