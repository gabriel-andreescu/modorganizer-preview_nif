#pragma once

#include <QString>
#include <QStringList>

// Hides loose files the way MO2 does, by renaming them with its hidden suffix, which its virtual file system skips.
namespace FileHiding {

struct RenameResult {
    // The renamed files' new paths, so the rename can be undone.
    QStringList renamed;
    // Empty on success. Nothing is renamed when it is set.
    QString error;
};

[[nodiscard]] QString HiddenPath(const QString& a_path);
[[nodiscard]] bool IsHidden(const QString& a_path);
// The path without the hidden suffix, so a copy keeps one identity while MO2 hides or restores it.
[[nodiscard]] QString VisiblePath(const QString& a_path);
[[nodiscard]] RenameResult Hide(const QStringList& a_files);
[[nodiscard]] RenameResult Restore(const QStringList& a_hiddenFiles);

}
