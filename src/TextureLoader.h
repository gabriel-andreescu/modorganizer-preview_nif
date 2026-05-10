#pragma once

#include "TextureSource.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>

class PreviewTexture;

namespace MOBase {
class IOrganizer;
}

class TextureLoader {
public:
    explicit TextureLoader(MOBase::IOrganizer* organizer, TextureSourceProvider textureSource = {});

    [[nodiscard]] std::unique_ptr<PreviewTexture> load(const QString& texturePath) const;
    [[nodiscard]] QByteArray loadDataFile(const QString& dataPath) const;

private:
    [[nodiscard]] std::unique_ptr<PreviewTexture> loadAuto(const QString& texturePath) const;
    [[nodiscard]] std::unique_ptr<PreviewTexture> tryLoadFromSource(const QString& texturePath) const;
    [[nodiscard]] static std::unique_ptr<PreviewTexture> loadLooseTexture(const QString& path);
    [[nodiscard]] static std::unique_ptr<PreviewTexture> tryLoadFromArchives(
        const QStringList& archivePaths,
        const QString& texturePath
    );
    [[nodiscard]] std::unique_ptr<PreviewTexture> tryLoadFromMods(const QString& texturePath) const;
    [[nodiscard]] std::unique_ptr<PreviewTexture> tryLoadFromGame(const QString& texturePath) const;
    [[nodiscard]] static std::unique_ptr<PreviewTexture> loadFromArchive(
        const QString& archivePath,
        const QString& texturePath
    );
    [[nodiscard]] QByteArray loadDataFileAuto(const QString& dataPath) const;
    [[nodiscard]] QByteArray tryLoadDataFileFromSource(const QString& dataPath) const;
    [[nodiscard]] static QByteArray tryLoadDataFileFromArchives(
        const QStringList& archivePaths,
        const QString& dataPath
    );
    [[nodiscard]] QByteArray tryLoadDataFileFromMods(const QString& dataPath) const;
    [[nodiscard]] QByteArray tryLoadDataFileFromGame(const QString& dataPath) const;
    [[nodiscard]] static QByteArray loadLooseDataFile(const QString& path);
    [[nodiscard]] static QByteArray loadDataFileFromArchive(const QString& archivePath, const QString& dataPath);

    MOBase::IOrganizer* m_MOInfo = nullptr;
    TextureSourceProvider m_TextureSource;
};
