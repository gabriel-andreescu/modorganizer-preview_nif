#include "Textures/Loader.h"
#include "Data/FileHiding.h"
#include "Data/GameFiles.h"
#include "Textures/Dds.h"
#include "Textures/PreviewTexture.h"
#include "Textures/Upload.h"

#include <QDebug>

#include <cstddef>
#include <exception>
#include <utility>

namespace {
struct CopyChoice {
    qsizetype index = -1;
    bool picked = false;
};

CopyChoice ChooseCopy(
    const TextureSelection& a_selection,
    const QString& a_key,
    const QVector<DataFileSource>& a_copies
) {
    if (const auto picked = a_selection.pickedCopies.constFind(a_key); picked != a_selection.pickedCopies.cend()) {
        const auto pickedPath = FileHiding::VisiblePath(*picked);
        for (qsizetype i = 0; i < a_copies.size(); ++i) {
            if (FileHiding::VisiblePath(a_copies[i].path) == pickedPath) {
                return {.index = i, .picked = true};
            }
        }
    }

    return {.index = SourceCopyIndex(a_selection.source, a_copies), .picked = false};
}

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

TextureLoader::TextureLoader(const GameFiles& a_gameFiles, TextureSelection a_selection)
    : _gameFiles {&a_gameFiles}
    , _selection {std::move(a_selection)} {}

LoadedTexture TextureLoader::Load(const QString& a_texturePath) const {
    auto [dataPath, copies] = FindTextureFiles(*_gameFiles, a_texturePath);
    copies.append(_gameFiles->HiddenSources(dataPath));
    auto file = Read(dataPath, std::move(copies), TextureDataPathKey(a_texturePath));
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
    return Read(a_dataPath, _gameFiles->Sources(a_dataPath), {});
}

LoadedDataFile TextureLoader::Read(
    const QString& a_dataPath,
    QVector<DataFileSource> a_copies,
    const QString& a_key
) const {
    const auto choice = ChooseCopy(_selection, a_key, a_copies);
    LoadedDataFile loaded;
    loaded.file.dataPath = a_dataPath;
    loaded.file.copies = std::move(a_copies);
    loaded.file.usedCopy = choice.index;
    loaded.file.picked = choice.picked;
    if (choice.index < 0) {
        return loaded;
    }

    const auto& source = loaded.file.copies[choice.index];
    loaded.file.chosenSourceLacksFile = !choice.picked && !ProviderSupplies(_selection.source, source);
    loaded.data = GameFiles::Read(source, a_dataPath);
    loaded.file.status = loaded.data.isEmpty() ? FileLoadStatus::kFailed : FileLoadStatus::kLoaded;
    return loaded;
}
