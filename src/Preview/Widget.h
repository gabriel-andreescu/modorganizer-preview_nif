#pragma once

#include "Preview/CameraSynchronizer.h"
#include "Preview/DialogChromeGuard.h"
#include "Preview/Preferences.h"
#include "Preview/Source.h"
#include "Render/Camera.h"

#include <QSharedPointer>
#include <QWidget>

#include <memory>

class CopyActions;
class GameFiles;
class NifPreviewPane;
class NoticeBar;
class QSplitter;
class ViewToolbar;

namespace MOBase {
class IOrganizer;
}

class NifPreviewWidget final : public QWidget {
    Q_OBJECT

public:
    NifPreviewWidget(
        NifPreviewResolution a_resolution,
        NifPreviewResolver a_resolver,
        MOBase::IOrganizer& a_organizer,
        QSharedPointer<Camera> a_camera,
        QWidget* a_parent = nullptr
    );
    ~NifPreviewWidget() override;
    NifPreviewWidget(const NifPreviewWidget&) = delete;
    NifPreviewWidget(NifPreviewWidget&&) = delete;
    NifPreviewWidget& operator=(const NifPreviewWidget&) = delete;
    NifPreviewWidget& operator=(NifPreviewWidget&&) = delete;

    [[nodiscard]] const QString& VirtualPath() const noexcept {
        return _sourceSet.virtualPath;
    }
    [[nodiscard]] NifPreviewPane* Pane(PreviewPaneSide a_side) const noexcept {
        return a_side == PreviewPaneSide::kLeft ? _leftPane : _rightPane;
    }
    [[nodiscard]] bool IsSplitViewEnabled() const;
    [[nodiscard]] bool IsCameraSyncEnabled() const;

protected:
    void showEvent(QShowEvent* a_event) override;

private:
    void ConnectToolbar();
    void ConnectPane(NifPreviewPane* a_pane);
    void ConnectCopyActions();
    void SetSplitEnabled(bool a_enabled);
    void SetShowCollision(bool a_show);
    void SetShowEditorMarkers(bool a_show);
    void SetShowTextures(bool a_show);
    void SetCameraSyncEnabled(bool a_enabled);
    void ResetCameras();
    void HandleTexturesLoaded();
    void UpdateComparison();
    void UpdateCollisionLock();
    void UpdateSplitAvailability();
    void Refresh();
    void InitializeRightPane();
    void HandleCameraMoved(NifPreviewPane* a_pane);
    void SyncCameraDelta(NifPreviewPane* a_sourcePane, const CameraState& a_newState);
    void UpdateCameraSnapshot(NifPreviewPane* a_pane);
    void UpdateCameraSnapshots();
    [[nodiscard]] PreviewPaneSide SideForPane(NifPreviewPane* a_pane) const;
    [[nodiscard]] int SecondaryProviderIndex() const;

    NifPreviewResolver _resolver;
    PreviewPreferences _preferences;
    std::shared_ptr<const GameFiles> _gameFiles;
    NifPreviewSourceSet _sourceSet;

    ViewToolbar* _toolbar = nullptr;
    NoticeBar* _notice = nullptr;
    QSplitter* _splitter = nullptr;
    NifPreviewPane* _leftPane = nullptr;
    NifPreviewPane* _rightPane = nullptr;
    CopyActions* _copyActions = nullptr;
    CameraSynchronizer _cameraSynchronizer;
    bool _applyingCameraSync = false;
    bool _rightPaneInitialized = false;
    PreviewDialogChromeGuard _hostChromeGuard;
};
