#pragma once

#include "Camera.h"
#include "NifPreviewSource.h"

#include <QPointer>
#include <QSharedPointer>
#include <QWidget>

class NifPreviewPane;
class QCheckBox;
class QFrame;
class QPushButton;
class QSplitter;

namespace MOBase
{
class IOrganizer;
}

class NifPreviewWidget final : public QWidget
{
  Q_OBJECT

public:
  NifPreviewWidget(NifPreviewSourceSet sourceSet, MOBase::IOrganizer* organizer,
                   QSharedPointer<Camera> camera, QWidget* parent = nullptr);
  ~NifPreviewWidget() override;

protected:
  void showEvent(QShowEvent* event) override;

private:
  struct HostChromeWidget
  {
    QPointer<QWidget> widget;
    bool wasVisible = false;
  };

  void setSplitViewEnabled(bool enabled);
  void setShowCollisionEnabled(bool enabled);
  void setCameraSyncEnabled(bool enabled);
  void resetCameras();
  void updateGlobalControls();
  void updateHostChrome();
  void restoreHostChrome();
  void captureHostChrome();
  void initializeRightPaneForSplit();
  void handleCameraMoved(NifPreviewPane* pane);
  void syncCameraDelta(NifPreviewPane* sourcePane, const CameraState& oldState,
                       const CameraState& newState);
  void updateCameraSnapshot(NifPreviewPane* pane);
  void updateCameraSnapshots();
  [[nodiscard]] int secondaryProviderIndex() const;

  NifPreviewSourceSet m_SourceSet;

  QFrame* m_GlobalControlsWidget   = nullptr;
  QPushButton* m_ResetCameraButton = nullptr;
  QCheckBox* m_ShowCollisionButton = nullptr;
  QCheckBox* m_SplitButton         = nullptr;
  QCheckBox* m_CameraSyncButton    = nullptr;
  QSplitter* m_Splitter            = nullptr;
  NifPreviewPane* m_LeftPane       = nullptr;
  NifPreviewPane* m_RightPane      = nullptr;
  CameraState m_LeftCameraState;
  CameraState m_RightCameraState;
  bool m_HasLeftCameraState   = false;
  bool m_HasRightCameraState  = false;
  bool m_ApplyingCameraSync   = false;
  bool m_RightPaneInitialized = false;

  QVector<HostChromeWidget> m_HostChrome;
  QPointer<QWidget> m_HostWindow;
};
