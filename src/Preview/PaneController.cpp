#include "Preview/PaneController.h"

#include <QDebug>
#include <QFileInfo>

#include <algorithm>
#include <exception>
#include <utility>

namespace {
QString PreviewTitleFor(const NifPreviewProvider& a_provider) {
    auto title = QFileInfo(a_provider.virtualPath).fileName();
    if (title.isEmpty()) {
        title = QFileInfo(a_provider.absolutePath).fileName();
    }
    if (title.isEmpty()) {
        title = a_provider.displayName;
    }

    return title;
}
} // namespace

PreviewPaneController::PreviewPaneController(MOBase::IOrganizer* a_organizer)
    : _organizer(a_organizer) {}

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

bool PreviewPaneController::SelectRelativeProvider(const int a_offset) {
    if (_providers.isEmpty()) {
        return false;
    }

    const auto providerCount = static_cast<int>(_providers.size());
    return SelectProvider((_currentProviderIndex + a_offset + providerCount) % providerCount);
}

bool PreviewPaneController::SelectTextureSource(const int a_index) {
    if (a_index < 0
        || a_index >= _textureSourceSet.providers.size()
        || a_index == _currentTextureSourceIndex
        || _textureSourceSet.providers.size() <= 2) {
        return false;
    }

    _currentTextureSourceIndex = a_index;
    return true;
}

bool PreviewPaneController::SelectRelativeTextureSource(const int a_offset) {
    if (_textureSourceSet.providers.size() <= 2) {
        return false;
    }

    const auto providerCount = static_cast<int>(_textureSourceSet.providers.size());
    return SelectTextureSource((_currentTextureSourceIndex + a_offset + providerCount) % providerCount);
}

PreviewPaneLoadResult PreviewPaneController::LoadCurrentProvider() {
    if (_currentProviderIndex < 0 || _currentProviderIndex >= _providers.size()) {
        ResetLoadedData();
        return {.status = PreviewPaneLoadStatus::kNoProvider};
    }

    const auto& provider = _providers[_currentProviderIndex];
    const auto title = PreviewTitleFor(provider);

    try {
        const auto nifFile = LoadNifProvider(provider);
        if (!nifFile) {
            qWarning("Failed to load NIF preview provider '%s'", qUtf8Printable(provider.displayName));
            ResetLoadedData();
            return {.status = PreviewPaneLoadStatus::kFailed, .title = title};
        }

        _currentNifFile = nifFile;
        _textureSourceSet = TextureSourceResolver::Resolve(_organizer, nifFile.get());
        _currentTextureSourceIndex = 0;
        return {.status = PreviewPaneLoadStatus::kLoaded, .title = title, .statsText = MakeNifStatsText(nifFile.get())};
    } catch (const std::exception& e) {
        qWarning("Failed to load NIF preview provider '%s': %s", qUtf8Printable(provider.displayName), e.what());
    } catch (...) {
        qWarning("Failed to load NIF preview provider '%s': unknown exception", qUtf8Printable(provider.displayName));
    }

    ResetLoadedData();
    return {.status = PreviewPaneLoadStatus::kFailed, .title = title};
}

TextureSourceProvider PreviewPaneController::CurrentTextureSourceProvider() const {
    if (_currentTextureSourceIndex < 0 || _currentTextureSourceIndex >= _textureSourceSet.providers.size()) {
        return {};
    }

    return _textureSourceSet.providers[_currentTextureSourceIndex];
}

void PreviewPaneController::ResetLoadedData() {
    _textureSourceSet = {};
    _currentTextureSourceIndex = 0;
    _currentNifFile.reset();
}
