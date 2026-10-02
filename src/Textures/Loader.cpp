#include "Textures/Loader.h"
#include "Data/GameFiles.h"
#include "Textures/Dds.h"
#include "Textures/PreviewTexture.h"
#include "Textures/Upload.h"

#include <QDebug>

#include <cstddef>
#include <exception>
#include <utility>

namespace {
std::unique_ptr<PreviewTexture> Decode(const LoadedDataFile& a_file) {
    const auto& dataPath = a_file.file.dataPath;
    const auto& source = a_file.file.copies[a_file.file.usedCopy].path;
    try {
        const auto texture = DdsTextures::LoadIfValid(
            a_file.data.constData(),
            static_cast<std::size_t>(a_file.data.size())
        );
        if (texture.empty()) {
            qWarning(
                "Failed to decode DDS '%s' from '%s': invalid or unsupported DDS",
                qUtf8Printable(dataPath),
                qUtf8Printable(source)
            );
            return nullptr;
        }
        return TextureUpload::Upload(texture);
    } catch (const std::exception& exception) {
        qWarning(
            "Failed to decode DDS '%s' from '%s': %s",
            qUtf8Printable(dataPath),
            qUtf8Printable(source),
            exception.what()
        );
        return nullptr;
    }
}
} // namespace

TextureLoader::TextureLoader(const GameFiles& a_gameFiles, TextureSourceProvider a_textureSource)
    : _gameFiles {&a_gameFiles}
    , _textureSource {std::move(a_textureSource)} {}

LoadedTexture TextureLoader::Load(const QString& a_texturePath) const {
    auto [dataPath, sources] = FindTextureFiles(*_gameFiles, a_texturePath);
    auto file = Read(dataPath, std::move(sources));
    if (file.file.status != FileLoadStatus::kLoaded) {
        return {.texture = nullptr, .file = std::move(file.file)};
    }

    auto texture = Decode(file);
    if (!texture) {
        file.file.status = FileLoadStatus::kFailed;
    }
    return {.texture = std::move(texture), .file = std::move(file.file)};
}

LoadedDataFile TextureLoader::LoadDataFile(const QString& a_dataPath) const {
    return Read(a_dataPath, _gameFiles->Sources(a_dataPath));
}

LoadedDataFile TextureLoader::Read(const QString& a_dataPath, QVector<DataFileSource> a_sources) const {
    LoadedDataFile loaded;
    loaded.file.dataPath = a_dataPath;
    loaded.file.usedCopy = ChooseTextureFile(_textureSource, a_sources);
    loaded.file.copies = std::move(a_sources);
    if (loaded.file.usedCopy < 0) {
        return loaded;
    }

    const auto& source = loaded.file.copies[loaded.file.usedCopy];
    loaded.file.chosenSourceLacksFile = !ProviderSupplies(_textureSource, source);
    loaded.data = GameFiles::Read(source, a_dataPath);
    loaded.file.status = loaded.data.isEmpty() ? FileLoadStatus::kFailed : FileLoadStatus::kLoaded;
    return loaded;
}
