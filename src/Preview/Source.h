#pragma once

#include <nifly/NifFile.hpp>

#include <QByteArray>
#include <QString>
#include <QVector>

#include <memory>

namespace MOBase {
class IOrganizer;
}

enum class NifPreviewProviderKind {
    kLooseFile,
    kArchive,
    kInMemory,
};

struct NifPreviewProvider {
    QString displayName;
    QString virtualPath;
    NifPreviewProviderKind kind = NifPreviewProviderKind::kLooseFile;
    QString absolutePath;
    QString archivePath;
    QString archiveName;
    QByteArray data;
};

struct NifPreviewSourceSet {
    QVector<NifPreviewProvider> providers;
    QString virtualPath;
    int currentIndex = 0;
};

class NifPreviewSourceResolver {
public:
    static NifPreviewSourceSet Resolve(
        MOBase::IOrganizer* a_organizer,
        const QString& a_fileName,
        const QByteArray& a_fileData
    );
};

std::shared_ptr<nifly::NifFile> LoadNifProvider(const NifPreviewProvider& a_provider);
QString MakeNifStatsText(const nifly::NifFile* a_nifFile);
