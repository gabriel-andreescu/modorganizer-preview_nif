#include "Data/GameFiles.h"
#include "Data/ArchiveAccess.h"
#include "Data/ArchiveIndex.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <limits>
#include <optional>
#include <ranges>
#include <uibase/imodinterface.h>
#include <uibase/imodlist.h>
#include <uibase/imoinfo.h>
#include <uibase/iplugingame.h>

#include <libbsarch/bs_archive.h>

namespace {
struct LooseOrigin {
    QString modName;
    QString directory;
    int priority = 0;
};

// MO2 names the game's Data folder "data" and the overwrite folder "overwrite". Data ranks below every mod and
// overwrite above them.
std::optional<LooseOrigin> OriginDirectory(const MOBase::IOrganizer& a_organizer, const QString& a_originName) {
    if (a_originName.compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0) {
        const auto* const game = a_organizer.managedGame();
        if (!game) {
            return std::nullopt;
        }
        return LooseOrigin {.modName = {}, .directory = game->dataDirectory().absolutePath(), .priority = -1};
    }

    if (a_originName.compare(QStringLiteral("overwrite"), Qt::CaseInsensitive) == 0) {
        return LooseOrigin {
            .modName = a_originName,
            .directory = a_organizer.overwritePath(),
            .priority = std::numeric_limits<int>::max(),
        };
    }

    const auto* const modList = a_organizer.modList();
    const auto* const mod = modList ? modList->getMod(a_originName) : nullptr;
    if (!mod) {
        return std::nullopt;
    }
    return LooseOrigin {
        .modName = a_originName,
        .directory = mod->absolutePath(),
        .priority = modList->priority(a_originName),
    };
}

QByteArray ReadLoose(const QString& a_path) {
    QFile file(a_path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read '%s': %s", qUtf8Printable(a_path), qUtf8Printable(file.errorString()));
        return {};
    }
    return file.readAll();
}

QByteArray ReadFromArchive(const QString& a_archivePath, const QString& a_dataPath) {
    libbsarch::bs_archive archive;
    QString error;
    if (!ArchiveAccess::LoadArchive(archive, a_archivePath, &error)) {
        qWarning("Failed to read archive '%s': %s", qUtf8Printable(a_archivePath), qUtf8Printable(error));
        return {};
    }

    ArchiveAccess::ExtractResult result;
    auto data = ArchiveAccess::ExtractBytes(archive, a_dataPath, std::numeric_limits<int>::max(), &result);
    if (result.status == ArchiveAccess::ExtractStatus::kError) {
        qWarning(
            "Failed to extract '%s' from '%s': %s",
            qUtf8Printable(a_dataPath),
            qUtf8Printable(a_archivePath),
            qUtf8Printable(result.error)
        );
    }
    return data;
}
} // namespace

GameFiles::GameFiles(const MOBase::IOrganizer& a_organizer, ArchiveIndex& a_archiveIndex)
    : _organizer(&a_organizer)
    , _archiveIndex(&a_archiveIndex)
    , _archives(LoadOrder::LoadedArchives(a_organizer))
    , _members(static_cast<std::size_t>(_archives.size())) {}

QVector<DataFileSource> GameFiles::Sources(const QString& a_dataPath) const {
    auto sources = LooseSources(a_dataPath);
    sources.append(ArchiveSources(a_dataPath));
    return sources;
}

QByteArray GameFiles::Read(const DataFileSource& a_source, const QString& a_dataPath) {
    return a_source.kind == DataFileKind::kLoose ? ReadLoose(a_source.path)
                                                 : ReadFromArchive(a_source.path, a_dataPath);
}

QString GameFiles::GameName() const {
    const auto* const game = _organizer->managedGame();
    return game ? game->displayGameName() : QString();
}

// MO2 also lists mods whose archives contain the file when archive parsing is on, so each origin is checked for a
// loose copy on disk.
QVector<DataFileSource> GameFiles::LooseSources(const QString& a_dataPath) const {
    std::vector<std::pair<int, DataFileSource>> ranked;
    for (const auto& originName : _organizer->getFileOrigins(a_dataPath)) {
        const auto origin = OriginDirectory(*_organizer, originName);
        if (!origin) {
            continue;
        }

        const QFileInfo file(QDir(origin->directory).absoluteFilePath(QDir::cleanPath(a_dataPath)));
        if (file.isFile()) {
            ranked.emplace_back(
                origin->priority,
                DataFileSource {
                    .kind = DataFileKind::kLoose,
                    .modName = origin->modName,
                    .path = QDir::fromNativeSeparators(file.absoluteFilePath()),
                    .plugin = {},
                }
            );
        }
    }

    std::ranges::stable_sort(ranked, std::ranges::greater {}, [](const auto& a_entry) { return a_entry.first; });
    QVector<DataFileSource> sources;
    for (auto& [priority, source] : ranked) {
        sources.push_back(std::move(source));
    }
    return sources;
}

QVector<DataFileSource> GameFiles::ArchiveSources(const QString& a_dataPath) const {
    QVector<DataFileSource> sources;
    for (auto index = _archives.size() - 1; index >= 0; --index) {
        const auto& archive = _archives[index];
        auto& members = _members[static_cast<std::size_t>(index)];
        if (!members) {
            members = _archiveIndex->Members(archive.path);
        }
        if (members->Contains(a_dataPath)) {
            sources.push_back({
                .kind = DataFileKind::kArchive,
                .modName = archive.modName,
                .path = archive.path,
                .plugin = archive.plugin,
            });
        }
    }
    return sources;
}
