#pragma once

#include <QString>

// Both paths are empty when no provider supplied the file.
struct DataOrigin {
    QString filePath;
    QString archivePath;
};
