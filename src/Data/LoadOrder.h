#pragma once

#include <QString>
#include <QVector>

namespace MOBase {
class IOrganizer;
}

struct LoadedArchive {
    QString path;
    QString modName;
    QString plugin;
};

namespace LoadOrder {

// The archives the game loads, in the order it registers them. A later archive's copy of a file wins over an earlier
// one's.
QVector<LoadedArchive> LoadedArchives(const MOBase::IOrganizer& a_organizer);

}
