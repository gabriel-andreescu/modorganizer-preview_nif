#include "TextureManager.h"
#include "TextureCache.h"
#include "TextureLoader.h"

#include <QDebug>
#include <QString>

#include <exception>
#include <memory>
#include <utility>

TextureManager::TextureManager(MOBase::IOrganizer* organizer, TextureSourceProvider textureSource)
    : m_Loader {std::make_unique<TextureLoader>(organizer, std::move(textureSource))}
    , m_Cache {std::make_unique<TextureCache>()} {}

TextureManager::~TextureManager() = default;

void TextureManager::cleanup() {
    m_Cache->cleanup();
}

PreviewTexture* TextureManager::getTexture(const std::string& texturePath) {
    return getTexture(QString::fromStdString(texturePath));
}

PreviewTexture* TextureManager::getTexture(const QString& texturePath) {
    if (texturePath.isEmpty()) {
        return nullptr;
    }

    if (m_Cache->containsTexture(texturePath)) {
        return m_Cache->texture(texturePath);
    }

    std::unique_ptr<PreviewTexture> texture;
    try {
        texture = m_Loader->load(texturePath);
    } catch (const std::exception& e) {
        qWarning("Failed to load NIF texture '%s': %s", qUtf8Printable(texturePath), e.what());
    } catch (...) {
        qWarning("Failed to load NIF texture '%s': unknown exception", qUtf8Printable(texturePath));
    }

    return m_Cache->storeTexture(texturePath, std::move(texture));
}

PreviewTexture* TextureManager::getErrorTexture() {
    return m_Cache->getErrorTexture();
}

PreviewTexture* TextureManager::getBlackTexture() {
    return m_Cache->getBlackTexture();
}

PreviewTexture* TextureManager::getWhiteTexture() {
    return m_Cache->getWhiteTexture();
}

PreviewTexture* TextureManager::getFlatNormalTexture() {
    return m_Cache->getFlatNormalTexture();
}
