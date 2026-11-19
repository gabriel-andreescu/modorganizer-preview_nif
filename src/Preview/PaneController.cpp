#include "Preview/PaneController.h"

#include <QDebug>

#include <algorithm>
#include <exception>
#include <utility>

PreviewPaneController::PreviewPaneController(const MOBase::IOrganizer* a_organizer, const GameFiles& a_gameFiles)
    : _organizer(a_organizer)
    , _gameFiles(&a_gameFiles) {}

void PreviewPaneController::SetGameFiles(const GameFiles& a_gameFiles) {
    _gameFiles = &a_gameFiles;
}

void PreviewPaneController::SetProviders(QVector<NifPreviewProvider> a_providers, const int a_currentIndex) {
    _providers = std::move(a_providers);
    const auto lastProviderIndex = static_cast<int>(_providers.size()) - 1;
    _currentProviderIndex = std::clamp(a_currentIndex, 0, std::max(0, lastProviderIndex));
    ResetLoadedData();
}

bool PreviewPaneController::SelectProvider(const int a_index) {
    if (a_index < 0 || a_index >= _providers.size() || a_index == _currentProviderIndex) {
        return false;
    }

    _currentProviderIndex = a_index;
    return true;
}

bool PreviewPaneController::SelectTextureSource(const int a_index) {
    if (a_index < 0 || a_index >= _textureSourceSet.providers.size()) {
        return false;
    }
    if (a_index == _currentTextureSourceIndex && _pickedCopies.isEmpty()) {
        return false;
    }

    _currentTextureSourceIndex = a_index;
    _pickedCopies.clear();
    return true;
}

void PreviewPaneController::PickTextureCopy(const QString& a_key, const QString& a_path) {
    _pickedCopies.insert(a_key, a_path);
}

void PreviewPaneController::ClearTexturePick(const QString& a_key) {
    _pickedCopies.remove(a_key);
}

PreviewPaneLoadResult PreviewPaneController::LoadCurrentProvider() {
    if (_currentProviderIndex < 0 || _currentProviderIndex >= _providers.size()) {
        ResetLoadedData();
        return {.status = PreviewPaneLoadStatus::kNoProvider};
    }

    const auto& provider = _providers[_currentProviderIndex];
    try {
        const auto nifFile = LoadNifProvider(provider);
        if (!nifFile) {
            qWarning("Failed to load NIF preview provider '%s'", qUtf8Printable(provider.displayName));
            ResetLoadedData();
            return {.status = PreviewPaneLoadStatus::kFailed};
        }

        _currentNifFile = nifFile;
        _textureSourceSet = TextureSourceResolver::Resolve(_organizer, *_gameFiles, nifFile.get());
        _currentTextureSourceIndex = 0;
        _pickedCopies.clear();
        return {.status = PreviewPaneLoadStatus::kLoaded, .statsText = MakeNifStatsText(nifFile.get())};
    } catch (const std::exception& e) {
        qWarning("Failed to load NIF preview provider '%s': %s", qUtf8Printable(provider.displayName), e.what());
    } catch (...) {
        qWarning("Failed to load NIF preview provider '%s': unknown exception", qUtf8Printable(provider.displayName));
    }

    ResetLoadedData();
    return {.status = PreviewPaneLoadStatus::kFailed};
}

void PreviewPaneController::RestoreTextureSelection(const TextureSelection& a_selection) {
    const auto& sources = _textureSourceSet.providers;
    const auto source = std::ranges::find_if(sources, [&](const TextureSourceProvider& a_source) {
        return a_source.kind == a_selection.source.kind && a_source.sourceName == a_selection.source.sourceName;
    });
    _currentTextureSourceIndex = source != sources.end() ? static_cast<int>(std::distance(sources.begin(), source)) : 0;
    _pickedCopies = a_selection.pickedCopies;
}

TextureSelection PreviewPaneController::CurrentTextureSelection() const {
    TextureSelection selection;
    selection.pickedCopies = _pickedCopies;
    if (_currentTextureSourceIndex >= 0 && _currentTextureSourceIndex < _textureSourceSet.providers.size()) {
        selection.source = _textureSourceSet.providers[_currentTextureSourceIndex];
    }
    return selection;
}

QString PreviewPaneController::CurrentDataPath() const {
    if (_currentProviderIndex < 0 || _currentProviderIndex >= _providers.size()) {
        return {};
    }

    return _providers[_currentProviderIndex].virtualPath;
}

void PreviewPaneController::ResetLoadedData() {
    _textureSourceSet = {};
    _currentTextureSourceIndex = 0;
    _pickedCopies.clear();
    _currentNifFile.reset();
}
