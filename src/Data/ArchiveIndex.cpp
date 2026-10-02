#include "Data/ArchiveIndex.h"
#include "Data/ArchiveAccess.h"

#include <QFileInfo>

#include <cstdint>
#include <exception>
#include <vector>

#include <libbsarch/bs_archive.h>

namespace {
std::vector<std::uint64_t> ReadMemberHashes(const QString& a_archivePath) {
    std::vector<std::uint64_t> hashes;
    libbsarch::bs_archive archive;
    QString error;
    if (!ArchiveAccess::LoadArchive(archive, a_archivePath, &error)) {
        qWarning("Failed to read archive '%s': %s", qUtf8Printable(a_archivePath), qUtf8Printable(error));
        return hashes;
    }

    try {
        const auto members = archive.list_files();
        hashes.reserve(members.size());
        for (const auto& member : members) {
            hashes.push_back(ArchiveMembers::Hash(QString::fromWCharArray(static_cast<const wchar_t*>(member))));
        }
    } catch (const std::exception& exception) {
        qWarning("Failed to list archive '%s': %s", qUtf8Printable(a_archivePath), exception.what());
    }
    return hashes;
}
} // namespace

std::shared_ptr<const ArchiveMembers> ArchiveIndex::Members(const QString& a_archivePath) {
    const QFileInfo file(a_archivePath);
    auto& entry = _entries[file.absoluteFilePath().toLower()];
    if (!entry.members || entry.size != file.size() || entry.modified != file.lastModified()) {
        entry = {
            .size = file.size(),
            .modified = file.lastModified(),
            .members = std::make_shared<const ArchiveMembers>(ReadMemberHashes(a_archivePath)),
        };
    }
    return entry.members;
}
