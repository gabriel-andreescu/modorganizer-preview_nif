#pragma once

#include "Camera.h"
#include "CameraSynchronizer.h"
#include "NifPreviewSource.h"
#include "PreviewDialogChromeGuard.h"

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
        NifPreviewSourceSet sourceSet,
        MOBase::IOrganizer* organizer,
        QSharedPointer<Camera> camera,
        QWidget* parent = nullptr
    );
    ~NifPreviewWidget() override;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setSplitViewEnabled(bool enabled, bool persistPreference);
    void setShowCollisionEnabled(bool enabled);
    void setCameraSyncEnabled(bool enabled);
    void resetCameras();
    void restoreSplitViewPreference();
    void saveSplitViewPreference(bool enabled) const;
    [[nodiscard]] bool isSplitViewEnabled() const;
    void updateGlobalControls();
    void updateHostChrome();
    void initializeRightPaneForSplit();
    void handleCameraMoved(NifPreviewPane* pane);
    void syncCameraDelta(NifPreviewPane* sourcePane, const CameraState& newState);
    void updateCameraSnapshot(NifPreviewPane* pane);
    void updateCameraSnapshots();
    [[nodiscard]] PreviewPaneSide sideForPane(NifPreviewPane* pane) const;
    [[nodiscard]] int secondaryProviderIndex() const;

    MOBase::IOrganizer* m_Organizer = nullptr;
    NifPreviewSourceSet m_SourceSet;

    QFrame* m_GlobalControlsWidget = nullptr;
    QPushButton* m_ResetCameraButton = nullptr;
    QCheckBox* m_ShowCollisionButton = nullptr;
    QCheckBox* m_SplitButton = nullptr;
    QCheckBox* m_CameraSyncButton = nullptr;
    QSplitter* m_Splitter = nullptr;
    NifPreviewPane* m_LeftPane = nullptr;
    NifPreviewPane* m_RightPane = nullptr;
    CameraSynchronizer m_CameraSynchronizer;
    bool m_ApplyingCameraSync = false;
    bool m_RightPaneInitialized = false;
    PreviewDialogChromeGuard m_HostChromeGuard;
};
