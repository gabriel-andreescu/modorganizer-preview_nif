#pragma once

#include "Preview/PaneController.h"
#include "Render/Camera.h"

#include <QSharedPointer>
#include <QWidget>

#include <memory>

class GameFiles;
class QComboBox;
class QLabel;
class QResizeEvent;
class QToolButton;
class QVBoxLayout;
class NifWidget;

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
    void SetCamera(QSharedPointer<Camera> a_camera);
    void SetShowCollision(bool a_showCollision);
    void SetShowEditorMarkers(bool a_showEditorMarkers);
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

signals:
    void ProviderChanged(int a_providerIndex);
    void CameraMoved();

protected:
    void resizeEvent(QResizeEvent* a_event) override;

private:
    void CreateControls();
    void CreateLayout();
    void ConnectControls();
    void SelectProvider(int a_index);
    void SelectRelativeProvider(int a_offset);
    void SelectTextureSource(int a_index);
    void SelectRelativeTextureSource(int a_offset);
    void UpdateControls();
    void UpdateSourceComboWidth();
    void UpdateTextureSourceComboItems();
    void UpdateTextureControls();
    void UpdateTextureSourceComboWidth();
    void LoadCurrentProvider();
    void ReloadCurrentNifWidget();
    void SetViewWidget(QWidget* a_widget);

    MOBase::IOrganizer* _organizer = nullptr;
    std::shared_ptr<const GameFiles> _gameFiles;
    PreviewPaneController _controller;
    bool _updatingControls = false;
    bool _updatingTextureControls = false;
    bool _showCollision = false;
    bool _showEditorMarkers = false;

    QSharedPointer<Camera> _camera;
    QMetaObject::Connection _cameraConnection;
    QLabel* _titleLabel = nullptr;
    QToolButton* _prevButton = nullptr;
    QComboBox* _sourceCombo = nullptr;
    QToolButton* _nextButton = nullptr;
    QLabel* _textureLabel = nullptr;
    QToolButton* _prevTextureButton = nullptr;
    QComboBox* _textureSourceCombo = nullptr;
    QToolButton* _nextTextureButton = nullptr;
    QLabel* _statsLabel = nullptr;
    QVBoxLayout* _viewLayout = nullptr;
    QWidget* _viewWidget = nullptr;
    NifWidget* _nifWidget = nullptr;
};
