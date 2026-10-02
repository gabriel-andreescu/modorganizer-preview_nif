#pragma once

#include <QString>

enum class DataFileKind {
    kLoose,
    kArchive,
};

// One copy of a data file the game can see, either loose or inside a loaded archive.
struct DataFileSource {
    DataFileKind kind = DataFileKind::kLoose;
    // The mod that supplies the file or archive. Empty for the game's own Data folder.
    QString modName;
    // The loose file, or the archive that contains the file.
    QString path;
    // The plugin whose archive this is. Empty for archives the game's INI files load.
    QString plugin;
};
