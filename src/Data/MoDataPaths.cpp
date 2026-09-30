#include "Data/MoDataPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <memory>
#include <ranges>
#include <uibase/game_features/dataarchives.h>
#include <uibase/game_features/igamefeatures.h>
#include <uibase/ifiletree.h>
#include <uibase/imodinterface.h>
#include <uibase/imoinfo.h>
#include <uibase/iplugingame.h>

namespace {
const MOBase::IProfile* ProfilePointer(const MOBase::IProfile* a_profile) {
    return a_profile;
}

template <class Profile>
const MOBase::IProfile* ProfilePointer(const std::shared_ptr<Profile>& a_profile) {
    return a_profile.get();
}

const MOBase::IProfile* CurrentProfile(const MOBase::IOrganizer* a_organizer) {
    if (!a_organizer) {
        return nullptr;
    }

    return ProfilePointer(a_organizer->profile());
}

void AppendUnique(QStringList& a_values, const QString& a_value) {
    if (!a_value.isEmpty() && !a_values.contains(a_value, Qt::CaseInsensitive)) {
        a_values.append(a_value);
    }
}

bool IsArchiveName(const QString& a_name) {
    return a_name.endsWith(".bsa", Qt::CaseInsensitive) || a_name.endsWith(".ba2", Qt::CaseInsensitive);
}

void AppendResolvedArchiveName(
    const MOBase::IOrganizer* a_organizer,
    QStringList& a_archivePaths,
    const QString& a_archiveName
) {
    AppendUnique(a_archivePaths, MoDataPaths::ResolveDataPath(a_organizer, a_archiveName));
}

void AppendResolvedArchiveNames(
    const MOBase::IOrganizer* a_organizer,
    QStringList& a_archivePaths,
    const QStringList& a_archiveNames
) {
    for (const auto& archiveName : std::ranges::reverse_view(a_archiveNames)) {
        AppendResolvedArchiveName(a_organizer, a_archivePaths, archiveName);
    }
}

void AppendGameDataArchiveFiles(const MOBase::IOrganizer* a_organizer, QStringList& a_archivePaths) {
    const auto* const game = a_organizer->managedGame();
    if (!game) {
        return;
    }

    const auto archives = game->dataDirectory().entryInfoList(
        {"*.bsa", "*.ba2"},
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase | QDir::Reversed
    );
    for (const auto& archiveInfo : archives) {
        AppendUnique(a_archivePaths, QDir::fromNativeSeparators(archiveInfo.absoluteFilePath()));
    }
}
} // namespace

namespace MoDataPaths {

QString ResolveDataPath(const MOBase::IOrganizer* a_organizer, const QString& a_path) {
    if (!a_organizer) {
        return {};
    }

    if (const auto resolved = a_organizer->resolvePath(a_path); !resolved.isEmpty()) {
        return QDir::fromNativeSeparators(QFileInfo(resolved).absoluteFilePath());
    }

    const auto* const game = a_organizer->managedGame();
    if (!game) {
        return {};
    }

    const auto dataPath = game->dataDirectory().absoluteFilePath(QDir::cleanPath(a_path));
    return QFileInfo::exists(dataPath) ? QDir::fromNativeSeparators(QFileInfo(dataPath).absoluteFilePath()) : QString();
}

QStringList ArchivePathsFromMod(const MOBase::IModInterface* a_mod) {
    QStringList archivePaths;
    if (!a_mod) {
        return archivePaths;
    }

    const auto fileTree = a_mod->fileTree();
    if (!fileTree) {
        return archivePaths;
    }

    for (auto it = fileTree->begin(); it != fileTree->end(); ++it) {
        const auto fileInfo = *it;
        if (!fileInfo || !fileInfo->isFile() || !IsArchiveName(fileInfo->name())) {
            continue;
        }

        const QFileInfo archiveInfo(QDir(a_mod->absolutePath()).filePath(fileInfo->name()));
        if (archiveInfo.exists() && archiveInfo.isFile()) {
            AppendUnique(archivePaths, QDir::fromNativeSeparators(archiveInfo.absoluteFilePath()));
        }
    }

    return archivePaths;
}

QStringList ArchivePathsFromGame(const MOBase::IOrganizer* a_organizer) {
    QStringList archivePaths;
    if (!a_organizer) {
        return archivePaths;
    }

    const auto* const features = a_organizer->gameFeatures();
    if (!features) {
        return archivePaths;
    }

    const auto gameArchives = features->gameFeature<MOBase::DataArchives>();
    if (!gameArchives) {
        return archivePaths;
    }

    AppendResolvedArchiveNames(a_organizer, archivePaths, gameArchives->archives(CurrentProfile(a_organizer)));
    AppendResolvedArchiveNames(a_organizer, archivePaths, gameArchives->vanillaArchives());
    AppendGameDataArchiveFiles(a_organizer, archivePaths);

    return archivePaths;
}

}
