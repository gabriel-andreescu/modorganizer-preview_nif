#include "TextureLoader.h"
#include "ArchiveAccess.h"
#include "DdsTextures.h"
#include "MoDataPaths.h"
#include "PreviewNif.h"
#include "PreviewTexture.h"
#include "TextureUpload.h"

#include <libbsarch/bs_archive.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstddef>
#include <exception>
#include <utility>

TextureLoader::TextureLoader(MOBase::IOrganizer* organizer, TextureSourceProvider textureSource)
    : m_MOInfo {organizer}
    , m_TextureSource {std::move(textureSource)} {}

std::unique_ptr<PreviewTexture> TextureLoader::load(const QString& texturePath) const {
    const auto normalizedPath = normalizeTextureDataPath(texturePath);
    if (auto texture = tryLoadFromSource(normalizedPath)) {
        return texture;
    }

    return loadAuto(normalizedPath);
}

QByteArray TextureLoader::loadDataFile(const QString& dataPath) const {
    if (auto data = tryLoadDataFileFromSource(dataPath); !data.isEmpty()) {
        return data;
    }

    return loadDataFileAuto(dataPath);
}

std::unique_ptr<PreviewTexture> TextureLoader::loadAuto(const QString& texturePath) const {
    if (texturePath.isEmpty()) {
        return nullptr;
    }

    if (!m_MOInfo) {
        qCritical("Failed to interface with Mod Organizer");
        return nullptr;
    }

    const auto* const game = m_MOInfo->managedGame();
    if (!game) {
        qCritical("Failed to interface with managed game plugin");
        return nullptr;
    }

    for (const auto& path : textureDataPathVariants(texturePath)) {
        const auto realPath = MoDataPaths::resolveDataPath(m_MOInfo, path);
        const bool fileExists = !realPath.isEmpty() && QFileInfo::exists(realPath) && QFileInfo(realPath).isFile();

        if (fileExists) {
            return loadLooseTexture(realPath);
        }
    }

    if (auto texture = tryLoadFromMods(texturePath)) {
        return texture;
    }

    if (auto texture = tryLoadFromGame(texturePath)) {
        return texture;
    }

    return nullptr;
}

std::unique_ptr<PreviewTexture> TextureLoader::tryLoadFromSource(const QString& texturePath) const {
    if (m_TextureSource.kind
        == TextureSourceProviderKind::Auto
        || !textureProviderCoversPath(m_TextureSource, texturePath)) {
        return nullptr;
    }

    switch (m_TextureSource.kind) {
        case TextureSourceProviderKind::Mod:
        case TextureSourceProviderKind::GameData: {
            if (!m_TextureSource.sourcePath.isEmpty()) {
                for (const auto& path : textureDataPathVariants(texturePath)) {
                    const auto realPath = QDir(m_TextureSource.sourcePath).absoluteFilePath(QDir::cleanPath(path));
                    if (QFileInfo::exists(realPath) && QFileInfo(realPath).isFile()) {
                        if (auto texture = loadLooseTexture(realPath)) {
                            return texture;
                        }
                    }
                }
            }

            return tryLoadFromArchives(m_TextureSource.archivePaths, texturePath);
        }
        case TextureSourceProviderKind::Auto: return nullptr;
    }

    return nullptr;
}

std::unique_ptr<PreviewTexture> TextureLoader::loadLooseTexture(const QString& path) {
    try {
        auto texture = DdsTextures::loadFileIfValid(path);
        if (texture.empty()) {
            qWarning("Failed to decode loose DDS '%s': invalid or unsupported DDS", qUtf8Printable(path));
            return nullptr;
        }
        return TextureUpload::upload(texture);
    } catch (const std::exception& e) {
        qWarning("Failed to decode loose DDS '%s': %s", qUtf8Printable(path), e.what());
        return nullptr;
    }
}

std::unique_ptr<PreviewTexture> TextureLoader::tryLoadFromArchives(
    const QStringList& archivePaths,
    const QString& texturePath
) {
    for (const auto& archivePath : archivePaths) {
        if (auto texture = loadFromArchive(archivePath, texturePath)) {
            return texture;
        }
    }

    return nullptr;
}

std::unique_ptr<PreviewTexture> TextureLoader::tryLoadFromMods(const QString& texturePath) const {
    if (!m_MOInfo) {
        return nullptr;
    }

    for (const auto& path : textureDataPathVariants(texturePath)) {
        const auto fileOrigins = m_MOInfo->getFileOrigins(path);
        if (fileOrigins.empty()) {
            continue;
        }

        const auto& modName = fileOrigins.constFirst();
        if (auto* const mod = m_MOInfo->modList()->getMod(modName)) {
            if (auto texture = tryLoadFromArchives(MoDataPaths::archivePathsFromMod(mod), path)) {
                return texture;
            }
        }
    }

    return nullptr;
}

std::unique_ptr<PreviewTexture> TextureLoader::tryLoadFromGame(const QString& texturePath) const {
    if (!m_MOInfo) {
        return nullptr;
    }

    return tryLoadFromArchives(MoDataPaths::archivePathsFromGame(m_MOInfo), texturePath);
}

std::unique_ptr<PreviewTexture> TextureLoader::loadFromArchive(const QString& archivePath, const QString& texturePath) {
    libbsarch::bs_archive archive;
    if (!ArchiveAccess::loadArchive(archive, archivePath)) {
        return nullptr;
    }

    const auto buffer = ArchiveAccess::extractBytes(archive, texturePath);
    if (buffer.isEmpty()) {
        return nullptr;
    }

    try {
        auto texture = DdsTextures::loadIfValid(buffer.constData(), static_cast<std::size_t>(buffer.size()));
        if (texture.empty()) {
            qWarning(
                "Failed to decode BSA DDS '%s' from '%s': invalid or unsupported DDS",
                qUtf8Printable(texturePath),
                qUtf8Printable(archivePath)
            );
            return nullptr;
        }
        return TextureUpload::upload(texture);
    } catch (const std::exception& e) {
        qWarning(
            "Failed to decode BSA DDS '%s' from '%s': %s",
            qUtf8Printable(texturePath),
            qUtf8Printable(archivePath),
            e.what()
        );
        return nullptr;
    }
}

QByteArray TextureLoader::loadDataFileAuto(const QString& dataPath) const {
    if (dataPath.isEmpty()) {
        return {};
    }

    if (!m_MOInfo) {
        qCritical("Failed to interface with Mod Organizer");
        return {};
    }

    const auto realPath = MoDataPaths::resolveDataPath(m_MOInfo, dataPath);
    const bool fileExists = !realPath.isEmpty() && QFileInfo::exists(realPath) && QFileInfo(realPath).isFile();

    if (fileExists) {
        return loadLooseDataFile(realPath);
    }

    if (auto data = tryLoadDataFileFromMods(dataPath); !data.isEmpty()) {
        return data;
    }

    return tryLoadDataFileFromGame(dataPath);
}

QByteArray TextureLoader::tryLoadDataFileFromSource(const QString& dataPath) const {
    if (m_TextureSource.kind == TextureSourceProviderKind::Auto) {
        return {};
    }

    switch (m_TextureSource.kind) {
        case TextureSourceProviderKind::Mod:
        case TextureSourceProviderKind::GameData: {
            if (!m_TextureSource.sourcePath.isEmpty()) {
                const auto realPath = QDir(m_TextureSource.sourcePath).absoluteFilePath(QDir::cleanPath(dataPath));
                if (QFileInfo::exists(realPath) && QFileInfo(realPath).isFile()) {
                    if (auto data = loadLooseDataFile(realPath); !data.isEmpty()) {
                        return data;
                    }
                }
            }

            return tryLoadDataFileFromArchives(m_TextureSource.archivePaths, dataPath);
        }
        case TextureSourceProviderKind::Auto: return {};
    }

    return {};
}

QByteArray TextureLoader::tryLoadDataFileFromArchives(const QStringList& archivePaths, const QString& dataPath) {
    for (const auto& archivePath : archivePaths) {
        if (auto data = loadDataFileFromArchive(archivePath, dataPath); !data.isEmpty()) {
            return data;
        }
    }

    return {};
}

QByteArray TextureLoader::tryLoadDataFileFromMods(const QString& dataPath) const {
    if (!m_MOInfo) {
        return {};
    }

    const auto fileOrigins = m_MOInfo->getFileOrigins(dataPath);
    if (fileOrigins.empty()) {
        return {};
    }

    const auto& modName = fileOrigins.constFirst();
    if (auto* const mod = m_MOInfo->modList()->getMod(modName)) {
        return tryLoadDataFileFromArchives(MoDataPaths::archivePathsFromMod(mod), dataPath);
    }
    return {};
}

QByteArray TextureLoader::tryLoadDataFileFromGame(const QString& dataPath) const {
    if (!m_MOInfo) {
        return {};
    }

    return tryLoadDataFileFromArchives(MoDataPaths::archivePathsFromGame(m_MOInfo), dataPath);
}

QByteArray TextureLoader::loadLooseDataFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read loose data file '%s'", qUtf8Printable(path));
        return {};
    }

    return file.readAll();
}

QByteArray TextureLoader::loadDataFileFromArchive(const QString& archivePath, const QString& dataPath) {
    libbsarch::bs_archive archive;
    if (!ArchiveAccess::loadArchive(archive, archivePath)) {
        return {};
    }

    return ArchiveAccess::extractBytes(archive, dataPath);
}
