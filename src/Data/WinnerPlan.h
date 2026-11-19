#pragma once

#include "Data/DataFileSource.h"

#include <QString>
#include <QVector>

// The loose copies to hide or restore so the game loads a chosen copy of a file.
struct WinnerPlan {
    // Loose copies to hide. Every other loose copy in a mod or the overwrite folder is hidden, lower priority ones
    // too, so the choice survives reordering the mods.
    QVector<DataFileSource> toHide;
    // The chosen copy, when MO2 hid it.
    QVector<DataFileSource> toRestore;
    // Why hiding loose copies can't make the copy win. Empty when it can.
    QString blocker;
};

// Takes the copies in the order the game ranks them, so the first is the one it loads. Hidden copies may follow them.
[[nodiscard]] WinnerPlan PlanWinner(const QVector<DataFileSource>& a_copies, qsizetype a_index);
