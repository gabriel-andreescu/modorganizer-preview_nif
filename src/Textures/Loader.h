#pragma once

#include "Textures/Origin.h"
#include "Textures/Source.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>

class PreviewTexture;

namespace MOBase {
class IOrganizer;
}

struct LoadedTexture {
    std::unique_ptr<PreviewTexture> texture;
    DataOrigin origin;
};

struct LoadedDataFile {
    QByteArray data;
    DataOrigin origin;
};

class TextureLoader {
public:
    explicit TextureLoader(MOBase::IOrganizer* a_organizer, TextureSourceProvider a_textureSource = {});

    [[nodiscard]] LoadedTexture Load(const QString& a_texturePath) const;
    [[nodiscard]] LoadedDataFile LoadDataFile(const QString& a_dataPath) const;

private:
    [[nodiscard]] LoadedTexture LoadAuto(const QString& a_texturePath) const;
    [[nodiscard]] LoadedTexture TryLoadFromSource(const QString& a_texturePath) const;
    [[nodiscard]] LoadedTexture TryLoadLooseFromSource(const QString& a_texturePath) const;
    [[nodiscard]] static LoadedTexture LoadLooseTexture(const QString& a_path);
    [[nodiscard]] static LoadedTexture TryLoadFromArchives(
        const QStringList& a_archivePaths,
        const QString& a_texturePath
    );
    [[nodiscard]] LoadedTexture TryLoadFromMods(const QString& a_texturePath) const;
    [[nodiscard]] LoadedTexture TryLoadFromGame(const QString& a_texturePath) const;
    [[nodiscard]] static LoadedTexture LoadFromArchive(const QString& a_archivePath, const QString& a_texturePath);
    [[nodiscard]] LoadedDataFile LoadDataFileAuto(const QString& a_dataPath) const;
    [[nodiscard]] LoadedDataFile TryLoadDataFileFromSource(const QString& a_dataPath) const;
    [[nodiscard]] LoadedDataFile TryLoadLooseDataFileFromSource(const QString& a_dataPath) const;
    [[nodiscard]] static LoadedDataFile TryLoadDataFileFromArchives(
        const QStringList& a_archivePaths,
        const QString& a_dataPath
    );
    [[nodiscard]] LoadedDataFile TryLoadDataFileFromMods(const QString& a_dataPath) const;
    [[nodiscard]] LoadedDataFile TryLoadDataFileFromGame(const QString& a_dataPath) const;
    [[nodiscard]] static LoadedDataFile LoadLooseDataFile(const QString& a_path);
    [[nodiscard]] static LoadedDataFile LoadDataFileFromArchive(
        const QString& a_archivePath,
        const QString& a_dataPath
    );

    MOBase::IOrganizer* _moInfo = nullptr;
    TextureSourceProvider _textureSource;
};
