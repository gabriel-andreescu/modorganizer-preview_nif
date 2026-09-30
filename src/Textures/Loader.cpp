#include "Textures/Loader.h"
#include "Data/ArchiveAccess.h"
#include "Data/MoDataPaths.h"
#include "PreviewNif.h"
#include "Textures/Dds.h"
#include "Textures/PreviewTexture.h"
#include "Textures/Upload.h"

#include <libbsarch/bs_archive.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstddef>
#include <exception>
#include <utility>

TextureLoader::TextureLoader(MOBase::IOrganizer* a_organizer, TextureSourceProvider a_textureSource)
    : _moInfo {a_organizer}
    , _textureSource {std::move(a_textureSource)} {}

LoadedTexture TextureLoader::Load(const QString& a_texturePath) const {
    const auto normalizedPath = NormalizeTextureDataPath(a_texturePath);
    if (auto loaded = TryLoadFromSource(normalizedPath); loaded.texture) {
        return loaded;
    }

    return LoadAuto(normalizedPath);
}

LoadedDataFile TextureLoader::LoadDataFile(const QString& a_dataPath) const {
    if (auto loaded = TryLoadDataFileFromSource(a_dataPath); !loaded.data.isEmpty()) {
        return loaded;
    }

    return LoadDataFileAuto(a_dataPath);
}

LoadedTexture TextureLoader::LoadAuto(const QString& a_texturePath) const {
    if (a_texturePath.isEmpty()) {
        return {};
    }

    if (!_moInfo) {
        qCritical("Failed to interface with Mod Organizer");
        return {};
    }

    const auto* const game = _moInfo->managedGame();
    if (!game) {
        qCritical("Failed to interface with managed game plugin");
        return {};
    }

    for (const auto& path : TextureDataPathVariants(a_texturePath)) {
        const auto realPath = MoDataPaths::ResolveDataPath(_moInfo, path);
        const bool fileExists = !realPath.isEmpty() && QFileInfo::exists(realPath) && QFileInfo(realPath).isFile();

        if (fileExists) {
            return LoadLooseTexture(realPath);
        }
    }

    if (auto loaded = TryLoadFromMods(a_texturePath); loaded.texture) {
        return loaded;
    }

    return TryLoadFromGame(a_texturePath);
}

LoadedTexture TextureLoader::TryLoadFromSource(const QString& a_texturePath) const {
    if (_textureSource.kind == TextureSourceProviderKind::kAuto
        || !TextureProviderCoversPath(_textureSource, a_texturePath)) {
        return {};
    }

    if (auto loaded = TryLoadLooseFromSource(a_texturePath); loaded.texture) {
        return loaded;
    }

    return TryLoadFromArchives(_textureSource.archivePaths, a_texturePath);
}

LoadedTexture TextureLoader::TryLoadLooseFromSource(const QString& a_texturePath) const {
    if (_textureSource.sourcePath.isEmpty()) {
        return {};
    }

    const QDir sourceDirectory(_textureSource.sourcePath);
    for (const auto& path : TextureDataPathVariants(a_texturePath)) {
        const QFileInfo file(sourceDirectory.absoluteFilePath(QDir::cleanPath(path)));
        if (!file.isFile()) {
            continue;
        }

        if (auto loaded = LoadLooseTexture(file.absoluteFilePath()); loaded.texture) {
            return loaded;
        }
    }

    return {};
}

LoadedTexture TextureLoader::LoadLooseTexture(const QString& a_path) {
    try {
        const auto texture = DdsTextures::LoadFileIfValid(a_path);
        if (texture.empty()) {
            qWarning("Failed to decode loose DDS '%s': invalid or unsupported DDS", qUtf8Printable(a_path));
            return {};
        }
        return {.texture = TextureUpload::Upload(texture), .origin = {.filePath = a_path, .archivePath = {}}};
    } catch (const std::exception& e) {
        qWarning("Failed to decode loose DDS '%s': %s", qUtf8Printable(a_path), e.what());
        return {};
    }
}

LoadedTexture TextureLoader::TryLoadFromArchives(const QStringList& a_archivePaths, const QString& a_texturePath) {
    for (const auto& archivePath : a_archivePaths) {
        if (auto loaded = LoadFromArchive(archivePath, a_texturePath); loaded.texture) {
            return loaded;
        }
    }

    return {};
}

LoadedTexture TextureLoader::TryLoadFromMods(const QString& a_texturePath) const {
    if (!_moInfo) {
        return {};
    }

    for (const auto& path : TextureDataPathVariants(a_texturePath)) {
        const auto fileOrigins = _moInfo->getFileOrigins(path);
        if (fileOrigins.empty()) {
            continue;
        }

        const auto& modName = fileOrigins.constFirst();
        if (const auto* const mod = _moInfo->modList()->getMod(modName)) {
            if (auto loaded = TryLoadFromArchives(MoDataPaths::ArchivePathsFromMod(mod), path); loaded.texture) {
                return loaded;
            }
        }
    }

    return {};
}

LoadedTexture TextureLoader::TryLoadFromGame(const QString& a_texturePath) const {
    if (!_moInfo) {
        return {};
    }

    return TryLoadFromArchives(MoDataPaths::ArchivePathsFromGame(_moInfo), a_texturePath);
}

LoadedTexture TextureLoader::LoadFromArchive(const QString& a_archivePath, const QString& a_texturePath) {
    libbsarch::bs_archive archive;
    if (!ArchiveAccess::LoadArchive(archive, a_archivePath)) {
        return {};
    }

    const auto buffer = ArchiveAccess::ExtractBytes(archive, a_texturePath);
    if (buffer.isEmpty()) {
        return {};
    }

    try {
        const auto texture = DdsTextures::LoadIfValid(buffer.constData(), static_cast<std::size_t>(buffer.size()));
        if (texture.empty()) {
            qWarning(
                "Failed to decode BSA DDS '%s' from '%s': invalid or unsupported DDS",
                qUtf8Printable(a_texturePath),
                qUtf8Printable(a_archivePath)
            );
            return {};
        }
        return {.texture = TextureUpload::Upload(texture), .origin = {.filePath = {}, .archivePath = a_archivePath}};
    } catch (const std::exception& e) {
        qWarning(
            "Failed to decode BSA DDS '%s' from '%s': %s",
            qUtf8Printable(a_texturePath),
            qUtf8Printable(a_archivePath),
            e.what()
        );
        return {};
    }
}

LoadedDataFile TextureLoader::LoadDataFileAuto(const QString& a_dataPath) const {
    if (a_dataPath.isEmpty()) {
        return {};
    }

    if (!_moInfo) {
        qCritical("Failed to interface with Mod Organizer");
        return {};
    }

    const auto realPath = MoDataPaths::ResolveDataPath(_moInfo, a_dataPath);
    const bool fileExists = !realPath.isEmpty() && QFileInfo::exists(realPath) && QFileInfo(realPath).isFile();

    if (fileExists) {
        return LoadLooseDataFile(realPath);
    }

    if (auto loaded = TryLoadDataFileFromMods(a_dataPath); !loaded.data.isEmpty()) {
        return loaded;
    }

    return TryLoadDataFileFromGame(a_dataPath);
}

LoadedDataFile TextureLoader::TryLoadDataFileFromSource(const QString& a_dataPath) const {
    if (_textureSource.kind == TextureSourceProviderKind::kAuto) {
        return {};
    }

    if (auto loaded = TryLoadLooseDataFileFromSource(a_dataPath); !loaded.data.isEmpty()) {
        return loaded;
    }

    return TryLoadDataFileFromArchives(_textureSource.archivePaths, a_dataPath);
}

LoadedDataFile TextureLoader::TryLoadLooseDataFileFromSource(const QString& a_dataPath) const {
    if (_textureSource.sourcePath.isEmpty()) {
        return {};
    }

    const QFileInfo file(QDir(_textureSource.sourcePath).absoluteFilePath(QDir::cleanPath(a_dataPath)));
    return file.isFile() ? LoadLooseDataFile(file.absoluteFilePath()) : LoadedDataFile {};
}

LoadedDataFile TextureLoader::TryLoadDataFileFromArchives(
    const QStringList& a_archivePaths,
    const QString& a_dataPath
) {
    for (const auto& archivePath : a_archivePaths) {
        if (auto loaded = LoadDataFileFromArchive(archivePath, a_dataPath); !loaded.data.isEmpty()) {
            return loaded;
        }
    }

    return {};
}

LoadedDataFile TextureLoader::TryLoadDataFileFromMods(const QString& a_dataPath) const {
    if (!_moInfo) {
        return {};
    }

    const auto fileOrigins = _moInfo->getFileOrigins(a_dataPath);
    if (fileOrigins.empty()) {
        return {};
    }

    const auto& modName = fileOrigins.constFirst();
    if (const auto* const mod = _moInfo->modList()->getMod(modName)) {
        return TryLoadDataFileFromArchives(MoDataPaths::ArchivePathsFromMod(mod), a_dataPath);
    }
    return {};
}

LoadedDataFile TextureLoader::TryLoadDataFileFromGame(const QString& a_dataPath) const {
    if (!_moInfo) {
        return {};
    }

    return TryLoadDataFileFromArchives(MoDataPaths::ArchivePathsFromGame(_moInfo), a_dataPath);
}

LoadedDataFile TextureLoader::LoadLooseDataFile(const QString& a_path) {
    QFile file(a_path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read loose data file '%s'", qUtf8Printable(a_path));
        return {};
    }

    return {.data = file.readAll(), .origin = {.filePath = a_path, .archivePath = {}}};
}

LoadedDataFile TextureLoader::LoadDataFileFromArchive(const QString& a_archivePath, const QString& a_dataPath) {
    libbsarch::bs_archive archive;
    if (!ArchiveAccess::LoadArchive(archive, a_archivePath)) {
        return {};
    }

    auto data = ArchiveAccess::ExtractBytes(archive, a_dataPath);
    if (data.isEmpty()) {
        return {};
    }

    return {.data = std::move(data), .origin = {.filePath = {}, .archivePath = a_archivePath}};
}
