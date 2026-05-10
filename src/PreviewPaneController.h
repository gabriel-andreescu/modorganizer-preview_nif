#pragma once

#include "NifPreviewSource.h"
#include "TextureSource.h"

#include <memory>

namespace MOBase {
class IOrganizer;
}

enum class PreviewPaneLoadStatus {
    NoProvider,
    Failed,
    Loaded
};

struct PreviewPaneLoadResult {
    PreviewPaneLoadStatus status = PreviewPaneLoadStatus::NoProvider;
    QString title;
    QString statsText;
};

class PreviewPaneController final {
public:
    explicit PreviewPaneController(MOBase::IOrganizer* organizer);

    void setProviders(QVector<NifPreviewProvider> providers, int currentIndex);
    bool selectProvider(int index);
    bool selectRelativeProvider(int offset);
    bool selectTextureSource(int index);
    bool selectRelativeTextureSource(int offset);
    PreviewPaneLoadResult loadCurrentProvider();

    [[nodiscard]] const QVector<NifPreviewProvider>& providers() const {
        return m_Providers;
    }

    [[nodiscard]] int currentProviderIndex() const {
        return m_CurrentProviderIndex;
    }

    [[nodiscard]] const TextureSourceSet& textureSources() const {
        return m_TextureSourceSet;
    }

    [[nodiscard]] int currentTextureSourceIndex() const {
        return m_CurrentTextureSourceIndex;
    }

    [[nodiscard]] std::shared_ptr<nifly::NifFile> currentNifFile() const {
        return m_CurrentNifFile;
    }

    [[nodiscard]] TextureSourceProvider currentTextureSourceProvider() const;

private:
    void resetLoadedData();

    MOBase::IOrganizer* m_Organizer = nullptr;
    QVector<NifPreviewProvider> m_Providers;
    int m_CurrentProviderIndex = 0;
    TextureSourceSet m_TextureSourceSet;
    int m_CurrentTextureSourceIndex = 0;
    std::shared_ptr<nifly::NifFile> m_CurrentNifFile;
};
