#pragma once

#include "Data/DataFileSource.h"

#include <QString>
#include <QVector>

enum class FileLoadStatus {
    kMissing,
    kLoaded,
    // A copy exists but could not be read or decoded.
    kFailed,
};

// How the preview resolved one data file: every copy the game can see and the one the preview read.
struct FileLoad {
    QString dataPath;
    // In the order the game ranks them, so the first is the copy the game loads.
    QVector<DataFileSource> copies;
    // Index into copies, or -1 when there is no copy.
    qsizetype usedCopy = -1;
    // The chosen texture source has no copy, so the preview read the copy the game loads, from whichever mod or
    // archive supplies it.
    bool chosenSourceLacksFile = false;
    FileLoadStatus status = FileLoadStatus::kMissing;
};
