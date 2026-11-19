#pragma once

#include "Data/DataFileSource.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>

class GameFiles;

namespace nifly {
class NifFile;
}

namespace MOBase {
class IOrganizer;
}

enum class TextureSourceProviderKind : std::uint8_t {
    kAuto,
    kMod,
    kGameData,
};

struct TextureReference {
    int slot = 0;
    QString slotName;
    QString path;
    QString key;
};

// A mod or the game's own data that the preview prefers textures from. It supplies the textures it has, and the game's
// winner supplies the rest.
struct TextureSourceProvider {
    QString displayName;
    TextureSourceProviderKind kind = TextureSourceProviderKind::kAuto;
    QString sourceName;
    qsizetype coveredTextureCount = 0;
    qsizetype totalTextureCount = 0;
};

struct TextureSourceSet {
    QVector<TextureReference> references;
    QVector<TextureSourceProvider> providers;
};

// The copies of a texture under the first path variant any copy uses.
struct TextureFiles {
    QString dataPath;
    QVector<DataFileSource> sources;
};

class TextureSourceResolver {
public:
    static TextureSourceSet Resolve(
        const MOBase::IOrganizer* a_organizer,
        const GameFiles& a_gameFiles,
        const nifly::NifFile* a_nifFile
    );
};

QString NormalizeTextureDataPath(QString a_path);
QStringList TextureDataPathVariants(const QString& a_path);
QString TextureDataPathKey(const QString& a_path);
TextureFiles FindTextureFiles(const GameFiles& a_gameFiles, const QString& a_texturePath);
[[nodiscard]] bool ProviderSupplies(const TextureSourceProvider& a_provider, const DataFileSource& a_source);
// The index of the copy the provider supplies, else of the copy the game loads, or -1 when the game sees none.
// Takes the copies in game order, hidden ones after the rest.
[[nodiscard]] qsizetype SourceCopyIndex(
    const TextureSourceProvider& a_provider,
    const QVector<DataFileSource>& a_copies
);
