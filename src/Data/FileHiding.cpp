#include "Data/FileHiding.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <utility>
#include <vector>

namespace {
// MO2's default for the suffixes its virtual file system skips.
constexpr auto kHiddenSuffix = ".mohidden";

struct Rename {
    QString from;
    QString to;
};

QString Native(const QString& a_path) {
    return QDir::toNativeSeparators(a_path);
}

// Checks every rename before making any, so a conflict leaves all files as they were. MO2 asks before replacing an
// existing target, and the preview stops instead.
QString Conflict(const std::vector<Rename>& a_renames) {
    for (const auto& [from, to] : a_renames) {
        if (!QFileInfo::exists(from)) {
            return QObject::tr("'%1' no longer exists.").arg(Native(from));
        }
        if (QFileInfo::exists(to)) {
            return QObject::tr("'%1' already exists.").arg(Native(to));
        }
    }
    return {};
}

FileHiding::RenameResult Apply(const std::vector<Rename>& a_renames) {
    if (auto conflict = Conflict(a_renames); !conflict.isEmpty()) {
        return {.renamed = {}, .error = std::move(conflict)};
    }

    FileHiding::RenameResult result;
    for (const auto& [from, to] : a_renames) {
        QFile file(from);
        if (file.rename(to)) {
            result.renamed.append(to);
            continue;
        }

        result.error = QObject::tr("Failed to rename '%1' to '%2': %3")
                           .arg(Native(from), Native(to), file.errorString());
        for (auto done = a_renames.begin(); done->from != from; ++done) {
            if (!QFile::rename(done->to, done->from)) {
                qWarning("Failed to rename '%s' back to '%s'", qUtf8Printable(done->to), qUtf8Printable(done->from));
            }
        }
        result.renamed.clear();
        return result;
    }
    return result;
}
} // namespace

namespace FileHiding {

QString HiddenPath(const QString& a_path) {
    return a_path + kHiddenSuffix;
}

bool IsHidden(const QString& a_path) {
    return a_path.endsWith(kHiddenSuffix, Qt::CaseInsensitive);
}

QString VisiblePath(const QString& a_path) {
    return IsHidden(a_path) ? a_path.chopped(static_cast<qsizetype>(qstrlen(kHiddenSuffix))) : a_path;
}

RenameResult Hide(const QStringList& a_files) {
    std::vector<Rename> renames;
    for (const auto& file : a_files) {
        renames.push_back({.from = file, .to = HiddenPath(file)});
    }
    return Apply(renames);
}

RenameResult Restore(const QStringList& a_hiddenFiles) {
    std::vector<Rename> renames;
    for (const auto& file : a_hiddenFiles) {
        renames.push_back({.from = file, .to = VisiblePath(file)});
    }
    return Apply(renames);
}

}
