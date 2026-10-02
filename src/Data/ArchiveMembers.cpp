#include "Data/ArchiveMembers.h"

#include <QDir>

#include <algorithm>
#include <utility>

ArchiveMembers::ArchiveMembers(std::vector<std::uint64_t> a_hashes)
    : _hashes(std::move(a_hashes)) {
    std::ranges::sort(_hashes);
}

bool ArchiveMembers::Contains(const QString& a_dataPath) const {
    return std::ranges::binary_search(_hashes, Hash(a_dataPath));
}

// 64-bit FNV-1a. A lookup against a million members falsely matches about once in 10^13, and a false match only makes
// a file look present in an archive that then fails to extract it.
std::uint64_t ArchiveMembers::Hash(const QString& a_dataPath) {
    auto path = QDir::fromNativeSeparators(a_dataPath).trimmed().toLower();
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }

    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    std::uint64_t hash = kOffsetBasis;
    for (const auto character : std::as_const(path)) {
        hash = (hash ^ character.unicode()) * kPrime;
    }
    return hash;
}
