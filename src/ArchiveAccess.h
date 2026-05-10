#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <limits>

namespace libbsarch {
class bs_archive;
}

namespace ArchiveAccess {

enum class ExtractStatus {
    Missing,
    Found,
    Oversized,
    Error
};

struct ExtractResult {
    ExtractStatus status = ExtractStatus::Missing;
    QString path;
    QString error;
    std::uint32_t size = 0;
};

bool loadArchive(libbsarch::bs_archive& archive, const QString& archivePath, QString* error = nullptr);
bool containsDataPath(
    libbsarch::bs_archive& archive,
    const QString& dataPath,
    QString* errorPath = nullptr,
    QString* error = nullptr
);
QByteArray extractBytes(
    libbsarch::bs_archive& archive,
    const QString& dataPath,
    int maxSize = std::numeric_limits<int>::max(),
    ExtractResult* result = nullptr
);

}
