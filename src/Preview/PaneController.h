#pragma once

#include "Preview/Source.h"
#include "Textures/Selection.h"

#include <cstdint>
#include <memory>

class GameFiles;

namespace MOBase {
class IOrganizer;
}

enum class PreviewPaneLoadStatus : std::uint8_t {
    kNoProvider,
    kFailed,
    kLoaded,
};

struct PreviewPaneLoadResult {
    PreviewPaneLoadStatus status = PreviewPaneLoadStatus::kNoProvider;
    QString statsText;
};

class PreviewPaneController final {
public:
    PreviewPaneController(const MOBase::IOrganizer* a_organizer, const GameFiles& a_gameFiles);

    void SetGameFiles(const GameFiles& a_gameFiles);
    void SetProviders(QVector<NifPreviewProvider> a_providers, int a_currentIndex);
    bool SelectProvider(int a_index);
    bool SelectTextureSource(int a_index);
    void PickTextureCopy(const QString& a_key, const QString& a_path);
    void ClearTexturePick(const QString& a_key);
    PreviewPaneLoadResult LoadCurrentProvider();
    // Selects the texture source with this kind and name and restores the picks, as far as the loaded NIF has them.
    void RestoreTextureSelection(const TextureSelection& a_selection);

    [[nodiscard]] const QVector<NifPreviewProvider>& Providers() const {
        return _providers;
    }

    [[nodiscard]] int CurrentProviderIndex() const {
        return _currentProviderIndex;
    }

    [[nodiscard]] const TextureSourceSet& TextureSources() const {
        return _textureSourceSet;
    }

    [[nodiscard]] int CurrentTextureSourceIndex() const {
        return _currentTextureSourceIndex;
    }

    [[nodiscard]] std::shared_ptr<nifly::NifFile> CurrentNifFile() const {
        return _currentNifFile;
    }

    [[nodiscard]] TextureSelection CurrentTextureSelection() const;
    [[nodiscard]] QString CurrentDataPath() const;

private:
    void ResetLoadedData();

    const MOBase::IOrganizer* _organizer = nullptr;
    const GameFiles* _gameFiles = nullptr;
    QVector<NifPreviewProvider> _providers;
    int _currentProviderIndex = 0;
    TextureSourceSet _textureSourceSet;
    int _currentTextureSourceIndex = 0;
    QHash<QString, QString> _pickedCopies;
    std::shared_ptr<nifly::NifFile> _currentNifFile;
};
