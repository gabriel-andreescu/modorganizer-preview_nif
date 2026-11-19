#pragma once

#include "Data/DataFileSource.h"
#include "Data/LoadOrder.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

class ArchiveIndex;
class ArchiveMembers;

namespace MOBase {
class IOrganizer;
}

// The game's view of its data files at the moment this snapshot was taken: every copy of a file, ordered the way the
// game ranks them.
class GameFiles {
public:
    GameFiles(const MOBase::IOrganizer& a_organizer, ArchiveIndex& a_archiveIndex);

    // Loose copies first, highest mod priority first, then archive copies, last loaded first. The first one is the
    // copy the game loads.
    [[nodiscard]] QVector<DataFileSource> Sources(const QString& a_dataPath) const;
    // Loose copies MO2 hid by adding the hidden suffix, highest mod priority first. Each path names the hidden file.
    [[nodiscard]] QVector<DataFileSource> HiddenSources(const QString& a_dataPath) const;
    [[nodiscard]] static QByteArray Read(const DataFileSource& a_source, const QString& a_dataPath);
    [[nodiscard]] QString GameName() const;

private:
    [[nodiscard]] QVector<DataFileSource> LooseSources(const QString& a_dataPath, bool a_hidden) const;
    [[nodiscard]] QVector<DataFileSource> ArchiveSources(const QString& a_dataPath) const;

    const MOBase::IOrganizer* _organizer;
    ArchiveIndex* _archiveIndex;
    QVector<LoadedArchive> _archives;
    // Each archive's member list, fetched on first use so the snapshot checks every archive file once.
    mutable std::vector<std::shared_ptr<const ArchiveMembers>> _members;
};
