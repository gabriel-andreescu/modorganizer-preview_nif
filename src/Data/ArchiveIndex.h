#pragma once

#include "Data/ArchiveMembers.h"

#include <QDateTime>
#include <QHash>
#include <QString>

#include <memory>

// The member lists of every archive asked about, read once and kept until the archive file changes.
class ArchiveIndex {
public:
    // Never null. An archive that can't be read has no members.
    [[nodiscard]] std::shared_ptr<const ArchiveMembers> Members(const QString& a_archivePath);

private:
    struct Entry {
        qint64 size = 0;
        QDateTime modified;
        std::shared_ptr<const ArchiveMembers> members;
    };

    QHash<QString, Entry> _entries;
};
