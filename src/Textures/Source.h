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

struct TextureSourceProvider {
    QString displayName;
    TextureSourceProviderKind kind = TextureSourceProviderKind::kAuto;
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
    static TextureSourceSet Resolve(MOBase::IOrganizer* a_organizer, const nifly::NifFile* a_nifFile);
};

QString MakeTextureSummaryText(const TextureSourceSet& a_sourceSet);
QString MakeTextureToolTipText(const TextureSourceSet& a_sourceSet, int a_providerIndex);
QString NormalizeTextureDataPath(QString a_path);
QStringList TextureDataPathVariants(const QString& a_path);
QString TextureDataPathKey(const QString& a_path);
bool TextureProviderCoversPath(const TextureSourceProvider& a_provider, const QString& a_texturePath);
