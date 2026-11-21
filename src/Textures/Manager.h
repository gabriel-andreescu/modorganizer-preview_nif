#pragma once

#include "Nif/Fo4Material.h"
#include "Textures/FileLoad.h"
#include "Textures/Selection.h"

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
    explicit TextureManager(const GameFiles& a_gameFiles, TextureSelection a_selection = {});
    ~TextureManager();
    TextureManager(const TextureManager&) = delete;
    TextureManager(TextureManager&&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager& operator=(TextureManager&&) = delete;

    void Cleanup();

    PreviewTexture* GetTexture(const std::string& a_texturePath);
    PreviewTexture* GetTexture(const QString& a_texturePath);
    [[nodiscard]] Fo4Material::Material GetFo4Material(const QString& a_materialPath);

    // Null for a path the manager never loaded.
    [[nodiscard]] const FileLoad* TextureFile(const QString& a_texturePath) const;
    [[nodiscard]] const FileLoad* MaterialFile(const QString& a_materialPath) const;

    PreviewTexture* GetErrorTexture();
    PreviewTexture* GetBlackTexture();
    PreviewTexture* GetWhiteTexture();
    PreviewTexture* GetFlatNormalTexture();

private:
    [[nodiscard]] static QString FileKey(const QString& a_normalizedPath);
    [[nodiscard]] const FileLoad* File(const QString& a_normalizedPath) const;

    std::unique_ptr<TextureLoader> _loader;
    std::unique_ptr<TextureCache> _cache;
    std::map<QString, FileLoad> _files;
};
