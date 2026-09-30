#pragma once

#include <QString>
#include <QStringList>

namespace MOBase {
class IModInterface;
class IOrganizer;
}

namespace MoDataPaths {

QString ResolveDataPath(const MOBase::IOrganizer* a_organizer, const QString& a_path);

// Returned in the mod's file-tree order, with paths resolved from the owning mod
// directory so duplicate archive names in different mods remain distinct.
QStringList ArchivePathsFromMod(const MOBase::IModInterface* a_mod);

// Returned in lookup-priority order: later profile archives first.
QStringList ArchivePathsFromGame(const MOBase::IOrganizer* a_organizer);

}
