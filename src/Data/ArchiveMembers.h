#pragma once

#include <QString>

#include <cstdint>
#include <vector>

// The member paths of one archive, kept as sorted 64-bit hashes of their lowercase forward-slash form so a large
// load order costs 8 bytes per member.
class ArchiveMembers {
public:
    explicit ArchiveMembers(std::vector<std::uint64_t> a_hashes);

    [[nodiscard]] bool Contains(const QString& a_dataPath) const;
    [[nodiscard]] static std::uint64_t Hash(const QString& a_dataPath);

private:
    std::vector<std::uint64_t> _hashes;
};
