#include "TextureManager.h"
#include "Fo4Material.h"
#include "TextureCache.h"
#include "TextureLoader.h"

#include <QDebug>
#include <QString>
#include <QStringList>

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
    const auto normalizedPath = normalizeTextureDataPath(texturePath);
    if (normalizedPath.isEmpty()) {
        return nullptr;
    }

    if (m_Cache->containsTexture(normalizedPath)) {
        return m_Cache->texture(normalizedPath);
    }

    std::unique_ptr<PreviewTexture> texture;
    try {
        texture = m_Loader->load(normalizedPath);
    } catch (const std::exception& e) {
        qWarning("Failed to load NIF texture '%s': %s", qUtf8Printable(normalizedPath), e.what());
    } catch (...) {
        qWarning("Failed to load NIF texture '%s': unknown exception", qUtf8Printable(normalizedPath));
    }

    return m_Cache->storeTexture(normalizedPath, std::move(texture));
}

QStringList TextureManager::getFo4MaterialTextures(const QString& materialPath) const {
    const auto normalizedPath = Fo4Material::normalizeMaterialDataPath(materialPath);
    if (normalizedPath.isEmpty()) {
        return {};
    }

    const auto material = Fo4Material::read(m_Loader->loadDataFile(normalizedPath));
    return material.valid ? material.textures : QStringList {};
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
