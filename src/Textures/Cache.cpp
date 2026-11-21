#include "Textures/Cache.h"
#include "Textures/PreviewTexture.h"
#include "Textures/Upload.h"

#include <QString>

#include <utility>

void TextureCache::Cleanup() {
    for (auto& [key, texture] : _textures) {
        DestroyTexture(texture);
    }
    _textures.clear();

    DestroyTexture(_errorTexture);
    DestroyTexture(_blackTexture);
    DestroyTexture(_whiteTexture);
    DestroyTexture(_flatNormalTexture);
}

bool TextureCache::ContainsTexture(const QString& a_texturePath) const {
    return _textures.contains(CacheKey(a_texturePath));
}

PreviewTexture* TextureCache::Texture(const QString& a_texturePath) const {
    if (const auto entry = _textures.find(CacheKey(a_texturePath)); entry != _textures.end()) {
        return entry->second.get();
    }

    return nullptr;
}

PreviewTexture* TextureCache::StoreTexture(const QString& a_texturePath, std::unique_ptr<PreviewTexture> a_texture) {
    auto* const texturePtr = a_texture.get();
    _textures[CacheKey(a_texturePath)] = std::move(a_texture);
    return texturePtr;
}

PreviewTexture* TextureCache::GetErrorTexture() {
    return GetFallbackTexture(_errorTexture, {1.0F, 0.0F, 1.0F, 1.0F});
}

PreviewTexture* TextureCache::GetBlackTexture() {
    return GetFallbackTexture(_blackTexture, {0.0F, 0.0F, 0.0F, 1.0F});
}

PreviewTexture* TextureCache::GetWhiteTexture() {
    return GetFallbackTexture(_whiteTexture, {1.0F, 1.0F, 1.0F, 1.0F});
}

PreviewTexture* TextureCache::GetFlatNormalTexture() {
    return GetFallbackTexture(_flatNormalTexture, {0.5F, 0.5F, 1.0F, 1.0F});
}

std::wstring TextureCache::CacheKey(const QString& a_texturePath) {
    return a_texturePath.toLower().toStdWString();
}

void TextureCache::DestroyTexture(std::unique_ptr<PreviewTexture>& a_texture) {
    if (a_texture) {
        a_texture->DestroyWithCurrentContext();
        a_texture.reset();
    }
}

PreviewTexture* TextureCache::GetFallbackTexture(std::unique_ptr<PreviewTexture>& a_texture, const QVector4D a_color) {
    if (!a_texture) {
        a_texture = TextureUpload::MakeSolidColor(a_color);
    }

    return a_texture.get();
}
