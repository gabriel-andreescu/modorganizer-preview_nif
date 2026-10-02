#pragma once

#include "Nif/Fo4Material.h"
#include "Textures/Origin.h"
#include "Textures/Source.h"

#include <QString>

#include <map>
#include <memory>
#include <string>

class GameFiles;
class PreviewTexture;
class TextureCache;
class TextureLoader;

class TextureManager {
public:
    explicit TextureManager(const GameFiles& a_gameFiles, TextureSourceProvider a_textureSource = {});
    ~TextureManager();
    TextureManager(const TextureManager&) = delete;
    TextureManager(TextureManager&&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager& operator=(TextureManager&&) = delete;

    void Cleanup();

    PreviewTexture* GetTexture(const std::string& a_texturePath);
    PreviewTexture* GetTexture(const QString& a_texturePath);
    [[nodiscard]] Fo4Material::Material GetFo4Material(const QString& a_materialPath);

    [[nodiscard]] DataOrigin TextureOrigin(const QString& a_texturePath) const;
    [[nodiscard]] DataOrigin MaterialOrigin(const QString& a_materialPath) const;

    PreviewTexture* GetErrorTexture();
    PreviewTexture* GetBlackTexture();
    PreviewTexture* GetWhiteTexture();
    PreviewTexture* GetFlatNormalTexture();

private:
    [[nodiscard]] static QString OriginKey(const QString& a_normalizedPath);
    [[nodiscard]] DataOrigin Origin(const QString& a_normalizedPath) const;

    std::unique_ptr<TextureLoader> _loader;
    std::unique_ptr<TextureCache> _cache;
    std::map<QString, DataOrigin> _origins;
};
