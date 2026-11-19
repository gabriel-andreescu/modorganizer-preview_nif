#pragma once

#include "Textures/FileLoad.h"
#include "Textures/Selection.h"

#include <QByteArray>
#include <QString>

#include <memory>

class GameFiles;
class PreviewTexture;

struct LoadedTexture {
    std::unique_ptr<PreviewTexture> texture;
    FileLoad file;
};

struct LoadedDataFile {
    QByteArray data;
    FileLoad file;
};

// Loads the copy of each texture picked for it, else the copy the texture source supplies, else the game's winner.
class TextureLoader {
public:
    explicit TextureLoader(const GameFiles& a_gameFiles, TextureSelection a_selection = {});

    [[nodiscard]] LoadedTexture Load(const QString& a_texturePath) const;
    [[nodiscard]] LoadedDataFile LoadDataFile(const QString& a_dataPath) const;

private:
    [[nodiscard]] LoadedDataFile Read(
        const QString& a_dataPath,
        QVector<DataFileSource> a_copies,
        const QString& a_key
    ) const;

    const GameFiles* _gameFiles;
    TextureSelection _selection;
};
