#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

// The member paths of one archive, kept as sorted 64-bit hashes of their lowercase forward-slash form so a large
// load order costs 8 bytes per member. The meshes among them keep their paths as well, so they can be listed.
class ArchiveMembers {
public:
    ArchiveMembers(std::vector<std::uint64_t> a_hashes, QStringList a_meshPaths);

    [[nodiscard]] bool Contains(const QString& a_dataPath) const;
    // The archive's meshes as the archive spells them, with forward slashes.
    [[nodiscard]] const QStringList& MeshPaths() const noexcept {
        return _meshPaths;
    }
    [[nodiscard]] static std::uint64_t Hash(const QString& a_dataPath);

private:
    std::vector<std::uint64_t> _hashes;
    QStringList _meshPaths;
};
