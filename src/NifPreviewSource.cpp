#include "NifPreviewSource.h"
#include "MoDataPaths.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QObject>

#include <algorithm>
#include <exception>
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
QString normalizeDataPath(QString path) {
    path = QDir::fromNativeSeparators(path).trimmed();
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }
    return path;
}

QString relativeToBase(const QString& basePath, const QString& fileName) {
    if (basePath.isEmpty()) {
        return {};
    }

    const auto relativePath = QDir(QDir::fromNativeSeparators(basePath)).relativeFilePath(fileName);
    if (relativePath.isEmpty() || relativePath.startsWith("../") || QDir::isAbsolutePath(relativePath)) {
        return {};
    }

    return normalizeDataPath(relativePath);
}

QString fallbackMeshesPath(const QString& fileName) {
    const auto normalized = QDir::fromNativeSeparators(fileName);
    const auto lower = normalized.toLower();

    if (lower.startsWith("meshes/")) {
        return normalizeDataPath(normalized);
    }

    const auto marker = QStringLiteral("/meshes/");
    const auto index = lower.indexOf(marker);
    if (index < 0) {
        return {};
    }

    return normalizeDataPath(normalized.mid(index + 1));
}

QString virtualPathFor(MOBase::IOrganizer* organizer, const QString& fileName) {
    const auto normalized = QDir::fromNativeSeparators(fileName);
    if (!QDir::isAbsolutePath(normalized)) {
        return normalizeDataPath(normalized);
    }

    if (organizer) {
        if (const auto* const game = organizer->managedGame()) {
            if (const auto relativePath = relativeToBase(game->dataDirectory().absolutePath(), normalized);
                !relativePath.isEmpty()) {
                return relativePath;
            }
        }

        if (auto* const modList = organizer->modList()) {
            for (const auto& modName : modList->allModsByProfilePriority()) {
                auto* const mod = modList->getMod(modName);
                if (!mod) {
                    continue;
                }

                if (const auto relativePath = relativeToBase(mod->absolutePath(), normalized);
                    !relativePath.isEmpty()) {
                    return relativePath;
                }
            }
        }

        if (const auto relativePath = relativeToBase(organizer->overwritePath(), normalized); !relativePath.isEmpty()) {
            return relativePath;
        }
    }

    return fallbackMeshesPath(normalized);
}

bool samePath(const QString& lhs, const QString& rhs) {
    return QFileInfo(lhs).absoluteFilePath().compare(QFileInfo(rhs).absoluteFilePath(), Qt::CaseInsensitive) == 0;
}

bool sameDataPath(const QString& lhs, const QString& rhs) {
    return normalizeDataPath(lhs).compare(normalizeDataPath(rhs), Qt::CaseInsensitive) == 0;
}

bool hasProviderPath(const QVector<NifPreviewProvider>& providers, const QString& path) {
    return std::ranges::any_of(providers, [&](const NifPreviewProvider& provider) {
        return provider.kind
               == NifPreviewProviderKind::LooseFile
               && !provider.absolutePath.isEmpty()
               && samePath(provider.absolutePath, path);
    });
}

bool hasArchiveProvider(
    const QVector<NifPreviewProvider>& providers,
    const QString& archivePath,
    const QString& virtualPath
) {
    return std::ranges::any_of(providers, [&](const NifPreviewProvider& provider) {
        return provider.kind
               == NifPreviewProviderKind::Archive
               && !provider.archivePath.isEmpty()
               && samePath(provider.archivePath, archivePath)
               && sameDataPath(provider.virtualPath, virtualPath);
    });
}

QByteArray extractArchiveFile(const QString& archivePath, const QString& virtualPath) {
    libbsarch::bs_archive archive;

    try {
        archive.load_from_disk(QDir::toNativeSeparators(archivePath).toStdWString());
    } catch (const std::exception& exception) {
        qWarning("Failed to load BSA archive '%s': %s", qUtf8Printable(archivePath), exception.what());
        return {};
    }

    const auto loadPath = [&](const QString& path) -> QByteArray {
        try {
            const auto blob = archive.extract_to_memory(path.toStdWString());
            if (!blob.data || blob.size == 0 || blob.size > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
                if (blob.size > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
                    qWarning(
                        "Skipping oversized NIF '%s' from BSA '%s'",
                        qUtf8Printable(virtualPath),
                        qUtf8Printable(archivePath)
                    );
                }
                return {};
            }

            return {static_cast<const char*>(blob.data), static_cast<int>(blob.size)};
        } catch (const std::exception&) {
            return {};
        }
    };

    const auto dataPath = normalizeDataPath(virtualPath);
    auto data = loadPath(dataPath);
    if (!data.isEmpty()) {
        return data;
    }

    const auto nativePath = QDir::toNativeSeparators(dataPath);
    return nativePath == dataPath ? QByteArray {} : loadPath(nativePath);
}

QString providerDisplayName(const QString& sourceName, const QString& archivePath) {
    auto archiveName = QFileInfo(archivePath).fileName();
    if (sourceName.isEmpty()) {
        return archiveName;
    }

    return QObject::tr("%1 - %2").arg(sourceName, archiveName);
}

void addLooseProvider(
    QVector<NifPreviewProvider>& providers,
    const QString& displayName,
    const QString& virtualPath,
    const QString& absolutePath
) {
    if (absolutePath.isEmpty() || !QFileInfo::exists(absolutePath) || hasProviderPath(providers, absolutePath)) {
        return;
    }

    providers.push_back(
        {.displayName = displayName,
            .virtualPath = virtualPath,
            .kind = NifPreviewProviderKind::LooseFile,
            .absolutePath = QDir::fromNativeSeparators(QFileInfo(absolutePath).absoluteFilePath()),
            .archivePath = {},
            .archiveName = {},
            .data = {}}
    );
}

void addArchiveProvider(
    QVector<NifPreviewProvider>& providers,
    const QString& sourceName,
    const QString& virtualPath,
    const QString& archivePath
) {
    const QFileInfo archiveInfo(archivePath);
    if (archivePath.isEmpty() || !archiveInfo.exists() || !archiveInfo.isFile()) {
        return;
    }

    const auto absoluteArchivePath = QDir::fromNativeSeparators(archiveInfo.absoluteFilePath());
    if (hasArchiveProvider(providers, absoluteArchivePath, virtualPath)) {
        return;
    }

    auto archiveData = extractArchiveFile(absoluteArchivePath, virtualPath);
    if (archiveData.isEmpty()) {
        return;
    }

    providers.push_back(
        {.displayName = providerDisplayName(sourceName, absoluteArchivePath),
            .virtualPath = virtualPath,
            .kind = NifPreviewProviderKind::Archive,
            .absolutePath = {},
            .archivePath = absoluteArchivePath,
            .archiveName = archiveInfo.fileName(),
            .data = std::move(archiveData)}
    );
}

void addArchiveProvidersFromMod(
    QVector<NifPreviewProvider>& providers,
    MOBase::IModInterface* mod,
    const QString& virtualPath
) {
    if (!mod) {
        return;
    }

    for (const auto& archivePath : MoDataPaths::archivePathsFromMod(mod)) {
        addArchiveProvider(providers, mod->name(), virtualPath, archivePath);
    }
}

void addGameArchiveProviders(
    QVector<NifPreviewProvider>& providers,
    MOBase::IOrganizer* organizer,
    const QString& virtualPath
) {
    if (!organizer) {
        return;
    }

    const auto* const game = organizer->managedGame();
    const auto sourceName = game ? game->displayGameName() : QObject::tr("Game Data");

    for (const auto& archivePath : MoDataPaths::archivePathsFromGame(organizer)) {
        addArchiveProvider(providers, sourceName, virtualPath, archivePath);
    }
}

int providerIndexForPath(const QVector<NifPreviewProvider>& providers, const QString& path) {
    if (path.isEmpty()) {
        return -1;
    }

    for (int i = 0; i < providers.size(); ++i) {
        if (!providers[i].absolutePath.isEmpty() && samePath(providers[i].absolutePath, path)) {
            return i;
        }
    }

    return -1;
}

int providerIndexForArchiveData(const QVector<NifPreviewProvider>& providers, const QByteArray& fileData) {
    if (fileData.isEmpty()) {
        return -1;
    }

    for (int i = 0; i < providers.size(); ++i) {
        const auto& provider = providers[i];
        if (provider.kind != NifPreviewProviderKind::Archive) {
            continue;
        }

        const auto archiveData = provider.data.isEmpty()
                                     ? extractArchiveFile(provider.archivePath, provider.virtualPath)
                                     : provider.data;
        if (archiveData.size() == fileData.size() && archiveData == fileData) {
            return i;
        }
    }

    return -1;
}

void addInMemoryProvider(
    QVector<NifPreviewProvider>& providers,
    const QString& displayName,
    const QString& virtualPath,
    const QString& fileName,
    const QByteArray& fileData
) {
    providers.insert(
        providers.begin(),
        {.displayName = displayName,
            .virtualPath = virtualPath,
            .kind = NifPreviewProviderKind::InMemory,
            .absolutePath = fileName,
            .archivePath = {},
            .archiveName = {},
            .data = fileData}
    );
}
} // namespace

NifPreviewSourceSet NifPreviewSourceResolver::resolve(
    MOBase::IOrganizer* organizer,
    const QString& fileName,
    const QByteArray& fileData
) {
    NifPreviewSourceSet sourceSet;
    sourceSet.virtualPath = virtualPathFor(organizer, fileName);

    const auto normalizedFileName = QDir::fromNativeSeparators(fileName);
    if (organizer && !sourceSet.virtualPath.isEmpty()) {
        if (auto* const modList = organizer->modList()) {
            const auto origins = organizer->getFileOrigins(sourceSet.virtualPath);
            for (const auto& modName : origins) {
                auto* const mod = modList->getMod(modName);
                if (!mod) {
                    continue;
                }

                const auto absolutePath = QDir(mod->absolutePath()).filePath(sourceSet.virtualPath);
                addLooseProvider(sourceSet.providers, mod->name(), sourceSet.virtualPath, absolutePath);
                addArchiveProvidersFromMod(sourceSet.providers, mod, sourceSet.virtualPath);
            }
        }

        addGameArchiveProviders(sourceSet.providers, organizer, sourceSet.virtualPath);
    }

    if (!fileData.isEmpty()) {
        if (const auto currentIndex = providerIndexForArchiveData(sourceSet.providers, fileData); currentIndex >= 0) {
            sourceSet.currentIndex = currentIndex;
        } else {
            addInMemoryProvider(
                sourceSet.providers,
                QObject::tr("Current Archive Preview"),
                sourceSet.virtualPath,
                normalizedFileName,
                fileData
            );
            sourceSet.currentIndex = 0;
        }
    }

    if (fileData.isEmpty() && QFileInfo::exists(normalizedFileName)) {
        addLooseProvider(
            sourceSet.providers,
            QFileInfo(normalizedFileName).fileName(),
            sourceSet.virtualPath,
            normalizedFileName
        );
        if (const auto currentIndex = providerIndexForPath(sourceSet.providers, normalizedFileName);
            currentIndex >= 0) {
            sourceSet.currentIndex = currentIndex;
        }
    }

    if (sourceSet.currentIndex < 0 || sourceSet.currentIndex >= sourceSet.providers.size()) {
        sourceSet.currentIndex = 0;
    }

    return sourceSet;
}

std::shared_ptr<nifly::NifFile> loadNifProvider(const NifPreviewProvider& provider) {
    std::shared_ptr<nifly::NifFile> nifFile;

    if (provider.kind == NifPreviewProviderKind::InMemory || provider.kind == NifPreviewProviderKind::Archive) {
        const auto data = provider.kind == NifPreviewProviderKind::Archive && provider.data.isEmpty()
                              ? extractArchiveFile(provider.archivePath, provider.virtualPath)
                              : provider.data;
        if (data.isEmpty()) {
            return nullptr;
        }

        const std::string bytes(data.constData(), static_cast<std::size_t>(data.size()));
        std::istringstream fileStream(bytes);
        nifFile = std::make_shared<nifly::NifFile>(fileStream);
    } else {
        nifFile = std::make_shared<nifly::NifFile>(std::filesystem::path(provider.absolutePath.toStdWString()));
    }

    if (!nifFile || !nifFile->IsValid()) {
        return nullptr;
    }

    return nifFile;
}

QString makeNifStatsText(const nifly::NifFile* nifFile) {
    unsigned int shapes = 0;
    unsigned int faces = 0;
    unsigned int verts = 0;

    if (nifFile) {
        for (const auto& shape : nifFile->GetShapes()) {
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
