#include "MoDataPaths.h"

#include <QDir>
#include <QFileInfo>

#include <memory>
#include <ranges>
#include <uibase/game_features/dataarchives.h>
#include <uibase/game_features/igamefeatures.h>
#include <uibase/ifiletree.h>
#include <uibase/imodinterface.h>
#include <uibase/imoinfo.h>
#include <uibase/iplugingame.h>

namespace {
const MOBase::IProfile* profilePointer(const MOBase::IProfile* profile) {
    return profile;
}

template <class Profile>
const MOBase::IProfile* profilePointer(const std::shared_ptr<Profile>& profile) {
    return profile.get();
}

const MOBase::IProfile* currentProfile(MOBase::IOrganizer* organizer) {
    if (!organizer) {
        return nullptr;
    }

    return profilePointer(organizer->profile());
}

void appendUnique(QStringList& values, const QString& value) {
    if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) {
        values.append(value);
    }
}

bool isArchiveName(const QString& name) {
    return name.endsWith(".bsa", Qt::CaseInsensitive) || name.endsWith(".ba2", Qt::CaseInsensitive);
}
} // namespace

namespace MoDataPaths {

QString resolveDataPath(MOBase::IOrganizer* organizer, const QString& path) {
    if (!organizer) {
        return {};
    }

    if (auto resolved = organizer->resolvePath(path); !resolved.isEmpty()) {
        return QDir::fromNativeSeparators(QFileInfo(resolved).absoluteFilePath());
    }

    const auto* const game = organizer->managedGame();
    if (!game) {
        return {};
    }

    const auto dataPath = game->dataDirectory().absoluteFilePath(QDir::cleanPath(path));
    return QFileInfo::exists(dataPath) ? QDir::fromNativeSeparators(QFileInfo(dataPath).absoluteFilePath()) : QString();
}

QStringList archivePathsFromMod(MOBase::IModInterface* mod) {
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
            appendUnique(archivePaths, QDir::fromNativeSeparators(archiveInfo.absoluteFilePath()));
        }
    }

    return archivePaths;
}

QStringList archivePathsFromGame(MOBase::IOrganizer* organizer) {
    QStringList archivePaths;
    if (!organizer) {
        return archivePaths;
    }

    auto* const features = organizer->gameFeatures();
    if (!features) {
        return archivePaths;
    }

    const auto gameArchives = features->gameFeature<MOBase::DataArchives>();
    if (!gameArchives) {
        return archivePaths;
    }

    for (
        auto archives = gameArchives->archives(currentProfile(organizer));
        const auto& archive : std::ranges::reverse_view(archives)
    ) {
        appendUnique(archivePaths, resolveDataPath(organizer, archive));
    }

    return archivePaths;
}

}
