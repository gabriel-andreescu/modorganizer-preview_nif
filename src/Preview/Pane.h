#pragma once

#include "Preview/PaneController.h"
#include "Render/Camera.h"
#include "Render/TextureResults.h"

#include <QSharedPointer>
#include <QWidget>

#include <memory>
#include <optional>

class GameFiles;
class NifWidget;
class QLabel;
class QVBoxLayout;
class TexturesPanel;
class TexturesSplit;
class VersionPicker;

namespace MOBase {
class IOrganizer;
}

class NifPreviewPane final : public QWidget {
    Q_OBJECT

public:
    NifPreviewPane(
        MOBase::IOrganizer* a_organizer,
        std::shared_ptr<const GameFiles> a_gameFiles,
        QWidget* a_parent = nullptr
    );

    void SetProviders(QVector<NifPreviewProvider> a_providers, int a_currentIndex);
    // Takes the files as MO2 sees them after a refresh, and keeps the shown version, texture source and picks.
    void Reload(std::shared_ptr<const GameFiles> a_gameFiles, QVector<NifPreviewProvider> a_providers);
    void SetCamera(QSharedPointer<Camera> a_camera);
    void SetShowCollision(bool a_showCollision);
    void SetShowEditorMarkers(bool a_showEditorMarkers);
    void SetTexturesOpen(bool a_open);
    void SetComparison(std::optional<QVector<TextureResult>> a_other);
    void ResetCamera();

    [[nodiscard]] QSharedPointer<Camera> SharedCamera() const {
        return _camera;
    }
    [[nodiscard]] int CurrentProviderIndex() const {
        return _controller.CurrentProviderIndex();
    }
    [[nodiscard]] const PreviewPaneController& Controller() const noexcept {
        return _controller;
    }
    [[nodiscard]] const NifWidget* View() const noexcept {
        return _nifWidget;
    }
    [[nodiscard]] NifWidget* View() noexcept {
        return _nifWidget;
    }
    [[nodiscard]] bool ShowsCollision() const noexcept {
        return _showCollision;
    }
    [[nodiscard]] bool ShowsEditorMarkers() const noexcept {
        return _showEditorMarkers;
    }
    // The loaded NIF has no meshes, so the view draws its collision whether or not collision is shown.
    [[nodiscard]] bool ShowsOnlyCollision() const noexcept {
        return _onlyCollision;
    }
    [[nodiscard]] bool TexturesOpen() const;
    // Empty until the view loads its textures.
    [[nodiscard]] QVector<TextureResult> TextureResults() const;

signals:
    void CameraMoved();
    // A view button turned this pane's camera to its view.
    void AxisViewShown();
    void TexturesLoaded();
    void HideOtherCopies(const QVector<DataFileSource>& a_copies, qsizetype a_index, const QString& a_label);
    void UnhideCopy(const DataFileSource& a_copy, const QString& a_label);

private:
    void ConnectControls();
    void SelectProvider(int a_index);
    void SelectTextureSource(int a_index);
    void PreviewTextureCopy(const QString& a_key, const QString& a_path);
    void HideOtherVersions(int a_index);
    void UnhideVersion(int a_index);
    void LoadCurrentProvider();
    void ReloadView();
    void UpdateTextureSources();
    void UpdateTexturesVisibility();
    void HandleTexturesLoaded();
    void SetViewWidget(QWidget* a_widget);

    MOBase::IOrganizer* _organizer = nullptr;
    std::shared_ptr<const GameFiles> _gameFiles;
    PreviewPaneController _controller;
    bool _showCollision = false;
    bool _showEditorMarkers = false;
    bool _texturesOpen = false;
    bool _onlyCollision = false;

    QSharedPointer<Camera> _camera;
    QMetaObject::Connection _cameraConnection;
    VersionPicker* _versionPicker = nullptr;
    QWidget* _viewport = nullptr;
    QVBoxLayout* _viewLayout = nullptr;
    TexturesPanel* _texturesPanel = nullptr;
    TexturesSplit* _texturesSplit = nullptr;
    QLabel* _statusLabel = nullptr;
    QWidget* _viewWidget = nullptr;
    NifWidget* _nifWidget = nullptr;
};
