#pragma once

#include "Preview/CameraSynchronizer.h"
#include "Preview/DialogChromeGuard.h"
#include "Preview/Source.h"
#include "Render/Camera.h"

#include <QSharedPointer>
#include <QWidget>

class NifPreviewPane;
class QCheckBox;
class QFrame;
class QPushButton;
class QSplitter;

namespace MOBase {
class IOrganizer;
}

class NifPreviewWidget final : public QWidget {
    Q_OBJECT

public:
    NifPreviewWidget(
        NifPreviewSourceSet a_sourceSet,
        MOBase::IOrganizer* a_organizer,
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
    void SetSplitViewEnabled(bool a_enabled, bool a_persistPreference);
    void SetShowCollisionEnabled(bool a_enabled);
    void SetShowEditorMarkersEnabled(bool a_enabled);
    void SetCameraSyncEnabled(bool a_enabled);
    void ResetCameras();
    void RestoreSplitViewPreference();
    void SaveSplitViewPreference(bool a_enabled) const;
    void UpdateGlobalControls();
    void UpdateHostChrome();
    void InitializeRightPaneForSplit();
    void HandleCameraMoved(NifPreviewPane* a_pane);
    void SyncCameraDelta(NifPreviewPane* a_sourcePane, const CameraState& a_newState);
    void UpdateCameraSnapshot(NifPreviewPane* a_pane);
    void UpdateCameraSnapshots();
    [[nodiscard]] PreviewPaneSide SideForPane(NifPreviewPane* a_pane) const;
    [[nodiscard]] int SecondaryProviderIndex() const;

    MOBase::IOrganizer* _organizer = nullptr;
    NifPreviewSourceSet _sourceSet;

    QFrame* _globalControlsWidget = nullptr;
    QPushButton* _resetCameraButton = nullptr;
    QCheckBox* _showCollisionButton = nullptr;
    QCheckBox* _showEditorMarkersButton = nullptr;
    QCheckBox* _splitButton = nullptr;
    QCheckBox* _cameraSyncButton = nullptr;
    QSplitter* _splitter = nullptr;
    NifPreviewPane* _leftPane = nullptr;
    NifPreviewPane* _rightPane = nullptr;
    CameraSynchronizer _cameraSynchronizer;
    bool _applyingCameraSync = false;
    bool _rightPaneInitialized = false;
    PreviewDialogChromeGuard _hostChromeGuard;
};
