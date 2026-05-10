#include "TextureCache.h"
#include "PreviewTexture.h"
#include "TextureUpload.h"

#include <QString>

#include <utility>

void TextureCache::cleanup() {
    for (auto& [key, texture] : m_Textures) {
        destroyTexture(texture);
    }
    m_Textures.clear();

    destroyTexture(m_ErrorTexture);
    destroyTexture(m_BlackTexture);
    destroyTexture(m_WhiteTexture);
    destroyTexture(m_FlatNormalTexture);
}

bool TextureCache::containsTexture(const QString& texturePath) const {
    return m_Textures.contains(cacheKey(texturePath));
}

PreviewTexture* TextureCache::texture(const QString& texturePath) const {
    if (const auto it = m_Textures.find(cacheKey(texturePath)); it != m_Textures.end()) {
        return it->second.get();
    }

    return nullptr;
}

PreviewTexture* TextureCache::storeTexture(const QString& texturePath, std::unique_ptr<PreviewTexture> texture) {
    auto* const texturePtr = texture.get();
    m_Textures[cacheKey(texturePath)] = std::move(texture);
    return texturePtr;
}

PreviewTexture* TextureCache::getErrorTexture() {
    return getFallbackTexture(m_ErrorTexture, {1.0f, 0.0f, 1.0f, 1.0f});
}

PreviewTexture* TextureCache::getBlackTexture() {
    return getFallbackTexture(m_BlackTexture, {0.0f, 0.0f, 0.0f, 1.0f});
}

PreviewTexture* TextureCache::getWhiteTexture() {
    return getFallbackTexture(m_WhiteTexture, {1.0f, 1.0f, 1.0f, 1.0f});
}

PreviewTexture* TextureCache::getFlatNormalTexture() {
    return getFallbackTexture(m_FlatNormalTexture, {0.5f, 0.5f, 1.0f, 1.0f});
}

std::wstring TextureCache::cacheKey(const QString& texturePath) {
    return texturePath.toLower().toStdWString();
}

void TextureCache::destroyTexture(std::unique_ptr<PreviewTexture>& texture) {
    if (texture) {
        texture->destroyWithCurrentContext();
        texture.reset();
    }
}

PreviewTexture* TextureCache::getFallbackTexture(std::unique_ptr<PreviewTexture>& texture, const QVector4D color) {
    if (!texture) {
        texture = TextureUpload::makeSolidColor(color);
    }

    return texture.get();
}
