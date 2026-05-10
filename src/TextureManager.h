#pragma once

#include "TextureSource.h"

#include <QStringList>

#include <memory>
#include <string>

class PreviewTexture;
class QString;
class TextureCache;
class TextureLoader;

class TextureManager {
public:
    explicit TextureManager(MOBase::IOrganizer* organizer, TextureSourceProvider textureSource = {});
    ~TextureManager();
    TextureManager(const TextureManager&) = delete;
    TextureManager(TextureManager&&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager& operator=(TextureManager&&) = delete;

    void cleanup();

    PreviewTexture* getTexture(const std::string& texturePath);
    PreviewTexture* getTexture(const QString& texturePath);
    [[nodiscard]] QStringList getFo4MaterialTextures(const QString& materialPath) const;

    PreviewTexture* getErrorTexture();
    PreviewTexture* getBlackTexture();
    PreviewTexture* getWhiteTexture();
    PreviewTexture* getFlatNormalTexture();

private:
    std::unique_ptr<TextureLoader> m_Loader;
    std::unique_ptr<TextureCache> m_Cache;
};
