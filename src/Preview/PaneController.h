#pragma once

#include "Preview/Source.h"
#include "Textures/Source.h"

#include <memory>

class GameFiles;

namespace MOBase {
class IOrganizer;
}

enum class PreviewPaneLoadStatus {
    kNoProvider,
    kFailed,
    kLoaded,
};

struct PreviewPaneLoadResult {
    PreviewPaneLoadStatus status = PreviewPaneLoadStatus::kNoProvider;
    QString title;
    QString statsText;
};

class PreviewPaneController final {
public:
    PreviewPaneController(const MOBase::IOrganizer* a_organizer, const GameFiles& a_gameFiles);

    void SetProviders(QVector<NifPreviewProvider> a_providers, int a_currentIndex);
    bool SelectProvider(int a_index);
    bool SelectRelativeProvider(int a_offset);
    bool SelectTextureSource(int a_index);
    bool SelectRelativeTextureSource(int a_offset);
    PreviewPaneLoadResult LoadCurrentProvider();

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

    [[nodiscard]] TextureSourceProvider CurrentTextureSourceProvider() const;
    [[nodiscard]] QString CurrentDataPath() const;

private:
    void ResetLoadedData();

    const MOBase::IOrganizer* _organizer = nullptr;
    const GameFiles* _gameFiles = nullptr;
    QVector<NifPreviewProvider> _providers;
    int _currentProviderIndex = 0;
    TextureSourceSet _textureSourceSet;
    int _currentTextureSourceIndex = 0;
    std::shared_ptr<nifly::NifFile> _currentNifFile;
};
