#include "Preview/Source.h"
#include "Data/GameFiles.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QStringList>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <uibase/imodinterface.h>
#include <uibase/imodlist.h>
#include <uibase/imoinfo.h>
#include <uibase/iplugingame.h>
#include <utility>

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

bool HasProviderPath(const QVector<NifPreviewProvider>& a_providers, const QString& a_path) {
    return std::ranges::any_of(a_providers, [&](const NifPreviewProvider& a_provider) {
        return a_provider.kind == NifPreviewProviderKind::kLooseFile
               && !a_provider.absolutePath.isEmpty()
               && SamePath(a_provider.absolutePath, a_path);
    });
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
    const DataFileSource& a_copy,
    const bool a_managed
) {
    if (a_copy.path.isEmpty() || !QFileInfo::exists(a_copy.path) || HasProviderPath(a_providers, a_copy.path)) {
        return;
    }

    a_providers.push_back({
        .displayName = a_displayName,
        .virtualPath = a_virtualPath,
        .kind = NifPreviewProviderKind::kLooseFile,
        .absolutePath = QDir::fromNativeSeparators(QFileInfo(a_copy.path).absoluteFilePath()),
        .archivePath = {},
        .archiveName = {},
        .data = {},
        .modName = a_copy.modName,
        .plugin = {},
        .hidden = a_copy.hidden,
        .managed = a_managed,
    });
}

void AddArchiveProvider(
    QVector<NifPreviewProvider>& a_providers,
    const QString& a_sourceName,
    const QString& a_virtualPath,
    const DataFileSource& a_copy,
    QByteArray a_data
) {
    if (a_data.isEmpty()) {
        return;
    }

    a_providers.push_back({
        .displayName = ProviderDisplayName(a_sourceName, a_copy.path),
        .virtualPath = a_virtualPath,
        .kind = NifPreviewProviderKind::kArchive,
        .absolutePath = {},
        .archivePath = a_copy.path,
        .archiveName = QFileInfo(a_copy.path).fileName(),
        .data = std::move(a_data),
        .modName = a_copy.modName,
        .plugin = a_copy.plugin,
        .hidden = false,
        .managed = true,
    });
}

void AddGameProviders(NifPreviewSourceSet& a_sourceSet, const GameFiles& a_gameFiles) {
    auto copies = a_gameFiles.Sources(a_sourceSet.virtualPath);
    copies.append(a_gameFiles.HiddenSources(a_sourceSet.virtualPath));
    for (const auto& copy : copies) {
        const auto owner = copy.modName.isEmpty() ? a_gameFiles.GameName() : copy.modName;
        if (copy.kind == DataFileKind::kLoose) {
            AddLooseProvider(a_sourceSet.providers, owner, a_sourceSet.virtualPath, copy, true);
        } else {
            AddArchiveProvider(
                a_sourceSet.providers,
                owner,
                a_sourceSet.virtualPath,
                copy,
                GameFiles::Read(copy, a_sourceSet.virtualPath)
            );
        }
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

        if (provider.data == a_fileData) {
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
            .modName = {},
            .plugin = {},
            .hidden = false,
            .managed = false,
        }
    );
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
    AddLooseProvider(
        a_sourceSet.providers,
        QFileInfo(a_fileName).fileName(),
        a_sourceSet.virtualPath,
        {.kind = DataFileKind::kLoose, .modName = {}, .path = a_fileName, .plugin = {}, .hidden = false},
        false
    );
    if (const auto currentIndex = ProviderIndexForPath(a_sourceSet.providers, a_fileName); currentIndex >= 0) {
        a_sourceSet.currentIndex = currentIndex;
    }
}
} // namespace

NifPreviewSourceSet NifPreviewSourceResolver::Resolve(
    const MOBase::IOrganizer* a_organizer,
    const GameFiles& a_gameFiles,
    const QString& a_fileName,
    const QByteArray& a_fileData
) {
    NifPreviewSourceSet sourceSet;
    sourceSet.virtualPath = VirtualPathFor(a_organizer, a_fileName);

    if (!sourceSet.virtualPath.isEmpty()) {
        AddGameProviders(sourceSet, a_gameFiles);
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
        const auto& data = a_provider.data;
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

QVector<DataFileSource> ProviderCopies(const QVector<NifPreviewProvider>& a_providers) {
    QVector<DataFileSource> copies;
    for (const auto& provider : a_providers) {
        if (!provider.managed) {
            continue;
        }

        const auto loose = provider.kind == NifPreviewProviderKind::kLooseFile;
        copies.push_back({
            .kind = loose ? DataFileKind::kLoose : DataFileKind::kArchive,
            .modName = provider.modName,
            .path = loose ? provider.absolutePath : provider.archivePath,
            .plugin = provider.plugin,
            .hidden = provider.hidden,
        });
    }
    return copies;
}

qsizetype ProviderCopyIndex(const QVector<NifPreviewProvider>& a_providers, const int a_providerIndex) {
    if (a_providerIndex < 0 || a_providerIndex >= a_providers.size() || !a_providers[a_providerIndex].managed) {
        return -1;
    }
    return std::count_if(a_providers.begin(), a_providers.begin() + a_providerIndex, [](const auto& a_provider) {
        return a_provider.managed;
    });
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
