#include "Textures/Manager.h"
#include "Textures/Cache.h"
#include "Textures/Loader.h"

#include <QDebug>

#include <exception>
#include <utility>

TextureManager::TextureManager(const GameFiles& a_gameFiles, TextureSourceProvider a_textureSource)
    : _loader {std::make_unique<TextureLoader>(a_gameFiles, std::move(a_textureSource))}
    , _cache {std::make_unique<TextureCache>()} {}

TextureManager::~TextureManager() = default;

void TextureManager::Cleanup() {
    _cache->Cleanup();
}

PreviewTexture* TextureManager::GetTexture(const std::string& a_texturePath) {
    return GetTexture(QString::fromStdString(a_texturePath));
}

PreviewTexture* TextureManager::GetTexture(const QString& a_texturePath) {
    const auto normalizedPath = NormalizeTextureDataPath(a_texturePath);
    if (normalizedPath.isEmpty()) {
        return nullptr;
    }

    if (_cache->ContainsTexture(normalizedPath)) {
        return _cache->Texture(normalizedPath);
    }

    LoadedTexture loaded {
        .texture = nullptr,
        .file = {.dataPath = normalizedPath, .status = FileLoadStatus::kFailed},
    };
    try {
        loaded = _loader->Load(normalizedPath);
    } catch (const std::exception& e) {
        qWarning("Failed to load NIF texture '%s': %s", qUtf8Printable(normalizedPath), e.what());
    } catch (...) {
        qWarning("Failed to load NIF texture '%s': unknown exception", qUtf8Printable(normalizedPath));
    }

    _files[FileKey(normalizedPath)] = std::move(loaded.file);
    return _cache->StoreTexture(normalizedPath, std::move(loaded.texture));
}

Fo4Material::Material TextureManager::GetFo4Material(const QString& a_materialPath) {
    const auto normalizedPath = Fo4Material::NormalizeMaterialDataPath(a_materialPath);
    if (normalizedPath.isEmpty()) {
        return {};
    }

    auto loaded = _loader->LoadDataFile(normalizedPath);
    auto material = Fo4Material::Read(loaded.data);
    if (!material.valid && loaded.file.status == FileLoadStatus::kLoaded) {
        loaded.file.status = FileLoadStatus::kFailed;
    }
    _files[FileKey(normalizedPath)] = std::move(loaded.file);
    return material;
}

const FileLoad* TextureManager::TextureFile(const QString& a_texturePath) const {
    return File(NormalizeTextureDataPath(a_texturePath));
}

const FileLoad* TextureManager::MaterialFile(const QString& a_materialPath) const {
    return File(Fo4Material::NormalizeMaterialDataPath(a_materialPath));
}

PreviewTexture* TextureManager::GetErrorTexture() {
    return _cache->GetErrorTexture();
}

PreviewTexture* TextureManager::GetBlackTexture() {
    return _cache->GetBlackTexture();
}

PreviewTexture* TextureManager::GetWhiteTexture() {
    return _cache->GetWhiteTexture();
}

PreviewTexture* TextureManager::GetFlatNormalTexture() {
    return _cache->GetFlatNormalTexture();
}

QString TextureManager::FileKey(const QString& a_normalizedPath) {
    return a_normalizedPath.toLower();
}

const FileLoad* TextureManager::File(const QString& a_normalizedPath) const {
    const auto entry = _files.find(FileKey(a_normalizedPath));
    return entry != _files.end() ? &entry->second : nullptr;
}
