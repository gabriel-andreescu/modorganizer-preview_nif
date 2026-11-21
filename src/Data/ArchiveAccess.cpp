#include "Data/ArchiveAccess.h"

#include <QDir>
#include <QStringList>

#include <exception>
#include <utility>

#include <libbsarch/bs_archive.h>

namespace {
QString NormalizeDataPath(QString a_path) {
    a_path = QDir::fromNativeSeparators(a_path).trimmed();
    while (a_path.startsWith('/')) {
        a_path.remove(0, 1);
    }
    return a_path;
}

void AppendUnique(QStringList& a_values, const QString& a_value) {
    if (!a_value.isEmpty() && !a_values.contains(a_value, Qt::CaseInsensitive)) {
        a_values.append(a_value);
    }
}

QStringList DataPathVariants(const QString& a_path) {
    const auto dataPath = NormalizeDataPath(a_path);
    QStringList paths;
    AppendUnique(paths, dataPath);
    AppendUnique(paths, QDir::toNativeSeparators(dataPath));
    return paths;
}

void SetResult(
    ArchiveAccess::ExtractResult* a_result,
    const ArchiveAccess::ExtractStatus a_status,
    const QString& a_path = {},
    const QString& a_error = {},
    const std::uint32_t a_size = 0
) {
    if (a_result) {
        a_result->status = a_status;
        a_result->path = a_path;
        a_result->error = a_error;
        a_result->size = a_size;
    }
}
} // namespace

namespace ArchiveAccess {

bool LoadArchive(libbsarch::bs_archive& a_archive, const QString& a_archivePath, QString* a_error) {
    try {
        a_archive.load_from_disk(QDir::toNativeSeparators(a_archivePath).toStdWString());
        if (a_error) {
            a_error->clear();
        }
        return true;
    } catch (const std::exception& exception) {
        if (a_error) {
            *a_error = QString::fromLocal8Bit(exception.what());
        }
        return false;
    }
}

bool ContainsDataPath(
    libbsarch::bs_archive& a_archive,
    const QString& a_dataPath,
    QString* a_errorPath,
    QString* a_error
) {
    if (a_errorPath) {
        a_errorPath->clear();
    }
    if (a_error) {
        a_error->clear();
    }

    for (const auto& path : DataPathVariants(a_dataPath)) {
        try {
            if (a_archive.find_file_record(path.toStdWString())) {
                return true;
            }
        } catch (const std::exception& exception) {
            if (a_errorPath) {
                *a_errorPath = path;
            }
            if (a_error) {
                *a_error = QString::fromLocal8Bit(exception.what());
            }
        }
    }

    return false;
}

QByteArray ExtractBytes(
    libbsarch::bs_archive& a_archive,
    const QString& a_dataPath,
    const int a_maxSize,
    ExtractResult* a_result
) {
    SetResult(a_result, ExtractStatus::kMissing);

    for (const auto& path : DataPathVariants(a_dataPath)) {
        try {
            const auto blob = a_archive.extract_to_memory(path.toStdWString());
            if (!blob.data || blob.size == 0) {
                continue;
            }

            if (std::cmp_greater(blob.size, a_maxSize)) {
                SetResult(a_result, ExtractStatus::kOversized, path, {}, blob.size);
                return {};
            }

            SetResult(a_result, ExtractStatus::kFound, path, {}, blob.size);
            return {static_cast<const char*>(blob.data), static_cast<int>(blob.size)};
        } catch (const std::exception& exception) {
            SetResult(a_result, ExtractStatus::kError, path, QString::fromLocal8Bit(exception.what()));
        }
    }

    if (a_result && a_result->status == ExtractStatus::kError) {
        return {};
    }

    SetResult(a_result, ExtractStatus::kMissing);
    return {};
}

}
