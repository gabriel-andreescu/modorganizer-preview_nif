#pragma once

#include <QString>
#include <QStringList>

namespace MOBase {
class IModInterface;
class IOrganizer;
}

namespace MoDataPaths {

QString resolveDataPath(MOBase::IOrganizer* organizer, const QString& path);

// Returned in the mod's file-tree order, with paths resolved from the owning mod
// directory so duplicate archive names in different mods remain distinct.
QStringList archivePathsFromMod(MOBase::IModInterface* mod);

// Returned in lookup-priority order: later profile archives first.
QStringList archivePathsFromGame(MOBase::IOrganizer* organizer);

}
