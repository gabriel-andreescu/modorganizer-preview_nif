#include "ArchiveAccess.h"

#include <QDir>
#include <QStringList>

#include <exception>
#include <utility>

#include <libbsarch/bs_archive.h>

namespace {
QString normalizeDataPath(QString path) {
    path = QDir::fromNativeSeparators(path).trimmed();
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }
    return path;
}

void appendUnique(QStringList& values, const QString& value) {
    if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) {
        values.append(value);
    }
}

QStringList dataPathVariants(const QString& path) {
    const auto dataPath = normalizeDataPath(path);
    QStringList paths;
    appendUnique(paths, dataPath);
    appendUnique(paths, QDir::toNativeSeparators(dataPath));
    return paths;
}

void setResult(
    ArchiveAccess::ExtractResult* result,
    const ArchiveAccess::ExtractStatus status,
    const QString& path = {},
    const QString& error = {},
    const std::uint32_t size = 0
) {
    if (result) {
        result->status = status;
        result->path = path;
        result->error = error;
        result->size = size;
    }
}
} // namespace

namespace ArchiveAccess {

bool loadArchive(libbsarch::bs_archive& archive, const QString& archivePath, QString* error) {
    try {
        archive.load_from_disk(QDir::toNativeSeparators(archivePath).toStdWString());
        if (error) {
            error->clear();
        }
        return true;
    } catch (const std::exception& exception) {
        if (error) {
            *error = QString::fromLocal8Bit(exception.what());
        }
        return false;
    }
}

bool containsDataPath(libbsarch::bs_archive& archive, const QString& dataPath, QString* errorPath, QString* error) {
    if (errorPath) {
        errorPath->clear();
    }
    if (error) {
        error->clear();
    }

    for (const auto& path : dataPathVariants(dataPath)) {
        try {
            if (archive.find_file_record(path.toStdWString())) {
                return true;
            }
        } catch (const std::exception& exception) {
            if (errorPath) {
                *errorPath = path;
            }
            if (error) {
                *error = QString::fromLocal8Bit(exception.what());
            }
        }
    }

    return false;
}

QByteArray extractBytes(
    libbsarch::bs_archive& archive,
    const QString& dataPath,
    const int maxSize,
    ExtractResult* result
) {
    setResult(result, ExtractStatus::Missing);

    for (const auto& path : dataPathVariants(dataPath)) {
        try {
            const auto blob = archive.extract_to_memory(path.toStdWString());
            if (!blob.data || blob.size == 0) {
                continue;
            }

            if (std::cmp_greater(blob.size, maxSize)) {
                setResult(result, ExtractStatus::Oversized, path, {}, blob.size);
                return {};
            }

            setResult(result, ExtractStatus::Found, path, {}, blob.size);
            return {static_cast<const char*>(blob.data), static_cast<int>(blob.size)};
        } catch (const std::exception& exception) {
            setResult(result, ExtractStatus::Error, path, QString::fromLocal8Bit(exception.what()));
        }
    }

    if (result && result->status == ExtractStatus::Error) {
        return {};
    }

    setResult(result, ExtractStatus::Missing);
    return {};
}

}
