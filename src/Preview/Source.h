#pragma once

#include "Data/DataFileSource.h"

#include <nifly/NifFile.hpp>

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdint>
#include <functional>
#include <memory>

class GameFiles;

namespace MOBase {
class IOrganizer;
}

enum class NifPreviewProviderKind : std::uint8_t {
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
    // The mod that supplies the file or archive. Empty for the game's own data and for in-memory previews.
    QString modName;
    QString plugin;
    // A loose copy MO2 hid, so the game doesn't see it.
    bool hidden = false;
    // One of the copies MO2 resolves for the data path, hidden ones included. False for in-memory previews and for
    // files outside the game's data, such as a disabled mod's.
    bool managed = false;
};

struct NifPreviewSourceSet {
    QVector<NifPreviewProvider> providers;
    QString virtualPath;
    int currentIndex = 0;
};

struct NifPreviewResolution {
    std::shared_ptr<const GameFiles> gameFiles;
    NifPreviewSourceSet sourceSet;
};

// Resolves a preview's versions against the files as MO2 sees them now.
using NifPreviewResolver = std::function<NifPreviewResolution()>;

class NifPreviewSourceResolver {
public:
    static NifPreviewSourceSet Resolve(
        const MOBase::IOrganizer* a_organizer,
        const GameFiles& a_gameFiles,
        const QString& a_fileName,
        const QByteArray& a_fileData
    );
};

std::shared_ptr<nifly::NifFile> LoadNifProvider(const NifPreviewProvider& a_provider);
// The managed providers' copies, in provider order, so the first one the game sees is the one it loads.
QVector<DataFileSource> ProviderCopies(const QVector<NifPreviewProvider>& a_providers);
// The provider's index in ProviderCopies, or -1 when it isn't managed.
qsizetype ProviderCopyIndex(const QVector<NifPreviewProvider>& a_providers, int a_providerIndex);
QString MakeNifStatsText(const nifly::NifFile* a_nifFile);
