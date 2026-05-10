#include "PreviewPaneController.h"

#include <QDebug>
#include <QFileInfo>

#include <algorithm>
#include <exception>
#include <utility>

namespace {
QString previewTitleFor(const NifPreviewProvider& provider) {
    auto title = QFileInfo(provider.virtualPath).fileName();
    if (title.isEmpty()) {
        title = QFileInfo(provider.absolutePath).fileName();
    }
    if (title.isEmpty()) {
        title = provider.displayName;
    }

    return title;
}
} // namespace

PreviewPaneController::PreviewPaneController(MOBase::IOrganizer* organizer)
    : m_Organizer(organizer) {}

void PreviewPaneController::setProviders(QVector<NifPreviewProvider> providers, const int currentIndex) {
    m_Providers = std::move(providers);
    const auto lastProviderIndex = static_cast<int>(m_Providers.size()) - 1;
    m_CurrentProviderIndex = std::clamp(currentIndex, 0, std::max(0, lastProviderIndex));
    resetLoadedData();
}

bool PreviewPaneController::selectProvider(const int index) {
    if (index < 0 || index >= m_Providers.size() || index == m_CurrentProviderIndex) {
        return false;
    }

    m_CurrentProviderIndex = index;
    return true;
}

bool PreviewPaneController::selectRelativeProvider(const int offset) {
    if (m_Providers.isEmpty()) {
        return false;
    }

    const auto providerCount = static_cast<int>(m_Providers.size());
    return selectProvider((m_CurrentProviderIndex + offset + providerCount) % providerCount);
}

bool PreviewPaneController::selectTextureSource(const int index) {
    if (index
        < 0
        || index
        >= m_TextureSourceSet.providers.size()
        || index
        == m_CurrentTextureSourceIndex
        || m_TextureSourceSet.providers.size()
        <= 2) {
        return false;
    }

    m_CurrentTextureSourceIndex = index;
    return true;
}

bool PreviewPaneController::selectRelativeTextureSource(const int offset) {
    if (m_TextureSourceSet.providers.size() <= 2) {
        return false;
    }

    const auto providerCount = static_cast<int>(m_TextureSourceSet.providers.size());
    return selectTextureSource((m_CurrentTextureSourceIndex + offset + providerCount) % providerCount);
}

PreviewPaneLoadResult PreviewPaneController::loadCurrentProvider() {
    if (m_CurrentProviderIndex < 0 || m_CurrentProviderIndex >= m_Providers.size()) {
        resetLoadedData();
        return {.status = PreviewPaneLoadStatus::NoProvider};
    }

    const auto& provider = m_Providers[m_CurrentProviderIndex];
    const auto title = previewTitleFor(provider);

    try {
        const auto nifFile = loadNifProvider(provider);
        if (!nifFile) {
            qWarning("Failed to load NIF preview provider '%s'", qUtf8Printable(provider.displayName));
            resetLoadedData();
            return {.status = PreviewPaneLoadStatus::Failed, .title = title};
        }

        m_CurrentNifFile = nifFile;
        m_TextureSourceSet = TextureSourceResolver::resolve(m_Organizer, nifFile.get());
        m_CurrentTextureSourceIndex = 0;
        return {.status = PreviewPaneLoadStatus::Loaded, .title = title, .statsText = makeNifStatsText(nifFile.get())};
    } catch (const std::exception& e) {
        qWarning("Failed to load NIF preview provider '%s': %s", qUtf8Printable(provider.displayName), e.what());
    } catch (...) {
        qWarning("Failed to load NIF preview provider '%s': unknown exception", qUtf8Printable(provider.displayName));
    }

    resetLoadedData();
    return {.status = PreviewPaneLoadStatus::Failed, .title = title};
}

TextureSourceProvider PreviewPaneController::currentTextureSourceProvider() const {
    if (m_CurrentTextureSourceIndex < 0 || m_CurrentTextureSourceIndex >= m_TextureSourceSet.providers.size()) {
        return {};
    }

    return m_TextureSourceSet.providers[m_CurrentTextureSourceIndex];
}

void PreviewPaneController::resetLoadedData() {
    m_TextureSourceSet = {};
    m_CurrentTextureSourceIndex = 0;
    m_CurrentNifFile.reset();
}
