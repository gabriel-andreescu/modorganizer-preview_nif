#pragma once

#include "Data/DataFileSource.h"

#include <QString>
#include <QVector>

// What hiding and unhiding can do for one copy of a file.
struct CopyActionState {
    bool canHideOthers = false;
    // What hiding the other copies does, or why it can't make this copy win.
    QString hideOthersToolTip;
    bool canUnhide = false;
};

// Takes the copies in game order, hidden ones after the rest, and the copy's index or -1 for none.
[[nodiscard]] CopyActionState CopyActionStateFor(const QVector<DataFileSource>& a_copies, qsizetype a_index);
