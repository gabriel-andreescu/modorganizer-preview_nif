#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <limits>

#include <libbsarch/bs_archive.h>

namespace ArchiveAccess {

enum class ExtractStatus : std::uint8_t {
    kMissing,
    kFound,
    kOversized,
    kError,
};

struct ExtractResult {
    ExtractStatus status = ExtractStatus::kMissing;
    QString path;
    QString error;
    std::uint32_t size = 0;
};

bool LoadArchive(libbsarch::bs_archive& a_archive, const QString& a_archivePath, QString* a_error = nullptr);
bool ContainsDataPath(
    libbsarch::bs_archive& a_archive,
    const QString& a_dataPath,
    QString* a_errorPath = nullptr,
    QString* a_error = nullptr
);
QByteArray ExtractBytes(
    libbsarch::bs_archive& a_archive,
    const QString& a_dataPath,
    int a_maxSize = std::numeric_limits<int>::max(),
    ExtractResult* a_result = nullptr
);

}
