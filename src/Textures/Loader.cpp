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
DataOrigin OriginOf(const DataFileSource& a_source) {
    if (a_source.kind == DataFileKind::kLoose) {
        return {.filePath = a_source.path, .archivePath = {}};
    }
    return {.filePath = {}, .archivePath = a_source.path};
}

std::unique_ptr<PreviewTexture> Decode(const LoadedDataFile& a_file, const QString& a_texturePath) {
    const auto& source = a_file.origin.filePath.isEmpty() ? a_file.origin.archivePath : a_file.origin.filePath;
    try {
        const auto texture = DdsTextures::LoadIfValid(
            a_file.data.constData(),
            static_cast<std::size_t>(a_file.data.size())
        );
        if (texture.empty()) {
            qWarning(
                "Failed to decode DDS '%s' from '%s': invalid or unsupported DDS",
                qUtf8Printable(a_texturePath),
                qUtf8Printable(source)
            );
            return nullptr;
        }
        return TextureUpload::Upload(texture);
    } catch (const std::exception& exception) {
        qWarning(
            "Failed to decode DDS '%s' from '%s': %s",
            qUtf8Printable(a_texturePath),
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
    const auto files = FindTextureFiles(*_gameFiles, a_texturePath);
    auto file = Read(files.dataPath, files.sources);
    if (file.data.isEmpty()) {
        return {};
    }

    auto texture = Decode(file, files.dataPath);
    auto origin = texture ? std::move(file.origin) : DataOrigin {};
    return {.texture = std::move(texture), .origin = std::move(origin)};
}

LoadedDataFile TextureLoader::LoadDataFile(const QString& a_dataPath) const {
    return Read(a_dataPath, _gameFiles->Sources(a_dataPath));
}

LoadedDataFile TextureLoader::Read(const QString& a_dataPath, const QVector<DataFileSource>& a_sources) const {
    const auto* const source = ChooseTextureFile(_textureSource, a_sources);
    if (!source) {
        return {};
    }
    return {.data = GameFiles::Read(*source, a_dataPath), .origin = OriginOf(*source)};
}
