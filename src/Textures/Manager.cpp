#include "Textures/Manager.h"
#include "Nif/Fo4Material.h"
#include "Textures/Cache.h"
#include "Textures/Loader.h"

#include <QDebug>

#include <exception>
#include <utility>

TextureManager::TextureManager(MOBase::IOrganizer* a_organizer, TextureSourceProvider a_textureSource)
    : _loader {std::make_unique<TextureLoader>(a_organizer, std::move(a_textureSource))}
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

    LoadedTexture loaded;
    try {
        loaded = _loader->Load(normalizedPath);
    } catch (const std::exception& e) {
        qWarning("Failed to load NIF texture '%s': %s", qUtf8Printable(normalizedPath), e.what());
    } catch (...) {
        qWarning("Failed to load NIF texture '%s': unknown exception", qUtf8Printable(normalizedPath));
    }

    _origins[OriginKey(normalizedPath)] = std::move(loaded.origin);
    return _cache->StoreTexture(normalizedPath, std::move(loaded.texture));
}

QStringList TextureManager::GetFo4MaterialTextures(const QString& a_materialPath) {
    const auto normalizedPath = Fo4Material::NormalizeMaterialDataPath(a_materialPath);
    if (normalizedPath.isEmpty()) {
        return {};
    }

    auto loaded = _loader->LoadDataFile(normalizedPath);
    _origins[OriginKey(normalizedPath)] = std::move(loaded.origin);
    const auto material = Fo4Material::Read(loaded.data);
    return material.valid ? material.textures : QStringList {};
}

DataOrigin TextureManager::TextureOrigin(const QString& a_texturePath) const {
    return Origin(NormalizeTextureDataPath(a_texturePath));
}

DataOrigin TextureManager::MaterialOrigin(const QString& a_materialPath) const {
    return Origin(Fo4Material::NormalizeMaterialDataPath(a_materialPath));
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

QString TextureManager::OriginKey(const QString& a_normalizedPath) {
    return a_normalizedPath.toLower();
}

DataOrigin TextureManager::Origin(const QString& a_normalizedPath) const {
    const auto entry = _origins.find(OriginKey(a_normalizedPath));
    return entry != _origins.end() ? entry->second : DataOrigin {};
}
