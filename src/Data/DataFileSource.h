#pragma once

#include <QString>

#include <cstdint>

enum class DataFileKind : std::uint8_t {
    kLoose,
    kArchive,
};

struct DataFileSource {
    DataFileKind kind = DataFileKind::kLoose;
    // The mod that supplies the file or archive. Empty for the game's own Data folder.
    QString modName;
    // The loose file, or the archive that contains the file.
    QString path;
    // The plugin whose archive this is. Empty for archives the game's INI files load.
    QString plugin;
    // A loose copy MO2 hid, so the game doesn't see it. The path names the hidden file.
    bool hidden = false;
};
