#pragma once

#include "Data/DataFileSource.h"

#include <QString>
#include <QVector>

#include <cstdint>

enum class FileLoadStatus : std::uint8_t {
    kMissing,
    kLoaded,
    // A copy exists but could not be read or decoded.
    kFailed,
};

struct FileLoad {
    QString dataPath;
    // In the order the game ranks them, so the first is the copy the game loads, followed by the copies MO2 hid.
    QVector<DataFileSource> copies;
    // Index into copies, or -1 when there is no copy.
    qsizetype usedCopy = -1;
    // The used copy was picked for this texture alone.
    bool picked = false;
    // The chosen texture source has no copy, so the preview read the copy the game loads, from whichever mod or
    // archive supplies it.
    bool chosenSourceLacksFile = false;
    FileLoadStatus status = FileLoadStatus::kMissing;
};
