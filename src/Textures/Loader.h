#pragma once

#include "Textures/FileLoad.h"
#include "Textures/Source.h"

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

// Loads the copy of each file the texture source supplies, or the game's winner when it supplies none.
class TextureLoader {
public:
    explicit TextureLoader(const GameFiles& a_gameFiles, TextureSourceProvider a_textureSource = {});

    [[nodiscard]] LoadedTexture Load(const QString& a_texturePath) const;
    [[nodiscard]] LoadedDataFile LoadDataFile(const QString& a_dataPath) const;

private:
    [[nodiscard]] LoadedDataFile Read(const QString& a_dataPath, QVector<DataFileSource> a_sources) const;

    const GameFiles* _gameFiles;
    TextureSourceProvider _textureSource;
};
