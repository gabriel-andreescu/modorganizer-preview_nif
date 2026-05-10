#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace nifly {
class NifFile;
}

namespace MOBase {
class IOrganizer;
}

enum class TextureSourceProviderKind {
    Auto,
    Mod,
    GameData
};

struct TextureReference {
    int slot = 0;
    QString slotName;
    QString path;
    QString key;
};

struct TextureSourceProvider {
    QString displayName;
    TextureSourceProviderKind kind = TextureSourceProviderKind::Auto;
    QString sourceName;
    QString sourcePath;
    QStringList archivePaths;
    QStringList coveredTextureKeys;
    qsizetype coveredTextureCount = 0;
    qsizetype totalTextureCount = 0;
};

struct TextureSourceSet {
    QVector<TextureReference> references;
    QVector<TextureSourceProvider> providers;
};

class TextureSourceResolver {
public:
    static TextureSourceSet resolve(MOBase::IOrganizer* organizer, const nifly::NifFile* nifFile);
};

QString makeTextureSummaryText(const TextureSourceSet& sourceSet);
QString makeTextureToolTipText(const TextureSourceSet& sourceSet, int providerIndex);
QString normalizeTextureDataPath(QString path);
QString textureDataPathKey(const QString& path);
bool textureProviderCoversPath(const TextureSourceProvider& provider, const QString& texturePath);
