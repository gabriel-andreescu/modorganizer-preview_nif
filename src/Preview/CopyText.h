#pragma once

#include "Data/DataFileSource.h"

#include <QString>
#include <QVector>

#include <cstdint>

namespace CopyText {

enum class Rank : std::uint8_t {
    kWinner,
    kOverwritten,
    kHidden,
};

// The mod, the overwrite folder or the game's own data that supplies the copy.
[[nodiscard]] QString Owner(const DataFileSource& a_copy);
// Takes the copies in game order.
[[nodiscard]] Rank RankOf(const QVector<DataFileSource>& a_copies, qsizetype a_index);
// The owner and, for an archived copy, its archive. One of the game's own archives goes by its file name alone. A copy
// the game doesn't load is marked overwritten or hidden.
[[nodiscard]] QString Label(const QVector<DataFileSource>& a_copies, qsizetype a_index);

}
