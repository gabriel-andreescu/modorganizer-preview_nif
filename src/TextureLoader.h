#pragma once

#include "TextureSource.h"

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

    MOBase::IOrganizer* m_MOInfo = nullptr;
    TextureSourceProvider m_TextureSource;
};
