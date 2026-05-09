#include "NifPreviewWidget.h"
#include "Camera.h"
#include "NifPreviewPane.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSharedPointer>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace
{
QSharedPointer<Camera> makePreviewCamera()
{
  return {new Camera(), &Camera::deleteLater};
}

float shortestAngleDelta(const float oldAngle, const float newAngle)
{
  auto delta = newAngle - oldAngle;
  while (delta > 180.0f) {
    delta -= 360.0f;
  }
  while (delta < -180.0f) {
    delta += 360.0f;
  }

  return delta;
}
}  // namespace

NifPreviewWidget::NifPreviewWidget(NifPreviewSourceSet sourceSet,
                                   MOBase::IOrganizer* organizer,
                                   QSharedPointer<Camera> camera, QWidget* parent)
    : QWidget(parent), m_SourceSet(std::move(sourceSet))
{
  m_GlobalControlsWidget = new QFrame(this);
  m_GlobalControlsWidget->setObjectName("nifPreviewGlobalToolbar");
  m_GlobalControlsWidget->setFrameShape(QFrame::StyledPanel);
  m_GlobalControlsWidget->setFrameShadow(QFrame::Raised);

  m_ResetCameraButton = new QPushButton(tr("Reset Camera"), m_GlobalControlsWidget);
  m_ResetCameraButton->setToolTip(tr("Reset the preview camera"));

  m_SplitButton = new QCheckBox(tr("Split Preview"), m_GlobalControlsWidget);
  m_SplitButton->setToolTip(tr("Compare two previewable versions of this NIF"));

  m_CameraSyncButton = new QCheckBox(tr("Sync Cameras"), m_GlobalControlsWidget);
  m_CameraSyncButton->setToolTip(tr("Synchronize cameras between preview panes"));
  m_CameraSyncButton->setChecked(true);

  const auto toolbarLayout = new QHBoxLayout(m_GlobalControlsWidget);
  toolbarLayout->setContentsMargins(8, 4, 8, 4);
  toolbarLayout->setSpacing(12);
  toolbarLayout->addWidget(m_ResetCameraButton);
  toolbarLayout->addWidget(m_SplitButton);
  toolbarLayout->addWidget(m_CameraSyncButton);
  toolbarLayout->addStretch(1);

  m_LeftPane  = new NifPreviewPane(organizer, this);
  m_RightPane = new NifPreviewPane(organizer, this);

  m_LeftPane->setCamera(std::move(camera));
  m_RightPane->setCamera(makePreviewCamera());
  m_LeftPane->setProviders(m_SourceSet.providers, m_SourceSet.currentIndex);
  m_RightPane->hide();

  m_Splitter = new QSplitter(Qt::Horizontal, this);
  m_Splitter->addWidget(m_LeftPane);
  m_Splitter->addWidget(m_RightPane);
  m_Splitter->setChildrenCollapsible(false);

  const auto rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(4);
  rootLayout->addWidget(m_GlobalControlsWidget);
  rootLayout->addWidget(m_Splitter, 1);

  connect(m_SplitButton, &QCheckBox::toggled, this,
          &NifPreviewWidget::setSplitViewEnabled);
  connect(m_CameraSyncButton, &QCheckBox::toggled, this,
          &NifPreviewWidget::setCameraSyncEnabled);
  connect(m_ResetCameraButton, &QPushButton::clicked, this,
          &NifPreviewWidget::resetCameras);
  connect(m_LeftPane, &NifPreviewPane::cameraMoved, this, [this]() {
    handleCameraMoved(m_LeftPane);
  });
  connect(m_RightPane, &NifPreviewPane::cameraMoved, this, [this]() {
    handleCameraMoved(m_RightPane);
  });

  updateGlobalControls();
}

NifPreviewWidget::~NifPreviewWidget()
{
  restoreHostChrome();
}

void NifPreviewWidget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  QTimer::singleShot(0, this, &NifPreviewWidget::updateHostChrome);
}

void NifPreviewWidget::setSplitViewEnabled(const bool enabled)
{
  if (enabled && m_SourceSet.providers.size() < 2) {
    m_SplitButton->setChecked(false);
    return;
  }

  if (enabled) {
    initializeRightPaneForSplit();
  }

  m_RightPane->setVisible(enabled);
  m_CameraSyncButton->setEnabled(enabled);

  if (enabled && m_CameraSyncButton->isChecked()) {
    updateCameraSnapshots();
  } else if (!enabled) {
    setCameraSyncEnabled(false);
  }

  updateGlobalControls();
  updateHostChrome();
}

void NifPreviewWidget::setCameraSyncEnabled(const bool enabled)
{
  if (!m_RightPane->isVisible() && enabled) {
    return;
  }

  if (enabled) {
    updateCameraSnapshots();
  }

  updateGlobalControls();
}

void NifPreviewWidget::resetCameras()
{
  m_ApplyingCameraSync = true;
  if (!m_RightPane->isVisible()) {
    m_LeftPane->resetCamera();
    m_ApplyingCameraSync = false;
    updateCameraSnapshot(m_LeftPane);
    return;
  }

  m_LeftPane->resetCamera();
  m_RightPane->resetCamera();
  m_ApplyingCameraSync = false;
  updateCameraSnapshots();
}

void NifPreviewWidget::updateGlobalControls()
{
  const auto canSplit = m_SourceSet.providers.size() > 1;
  m_SplitButton->setEnabled(canSplit);
  m_SplitButton->setToolTip(canSplit
                                ? tr("Compare two previewable versions of this NIF")
                                : tr("Split view requires at least two previewable "
                                     "versions of this NIF"));

  const auto splitActive = m_RightPane->isVisible();
  m_CameraSyncButton->setVisible(splitActive);
  m_CameraSyncButton->setEnabled(splitActive);

  m_ResetCameraButton->setText(splitActive ? tr("Reset Cameras") : tr("Reset Camera"));
  m_ResetCameraButton->setToolTip(splitActive ? tr("Reset both preview cameras")
                                              : tr("Reset the preview camera"));
}

void NifPreviewWidget::updateHostChrome()
{
  captureHostChrome();
  if (m_HostChrome.isEmpty()) {
    return;
  }

  for (const auto& hostWidget : m_HostChrome) {
    if (hostWidget.widget) {
      hostWidget.widget->hide();
    }
  }
}

void NifPreviewWidget::restoreHostChrome()
{
  for (const auto& hostWidget : m_HostChrome) {
    if (hostWidget.widget) {
      hostWidget.widget->setVisible(hostWidget.wasVisible);
    }
  }
  m_HostChrome.clear();
  m_HostWindow.clear();
}

void NifPreviewWidget::captureHostChrome()
{
  if (!m_HostChrome.isEmpty()) {
    return;
  }

  const auto hostWindow = window();
  if (!hostWindow || hostWindow == this ||
      hostWindow->objectName() != "PreviewDialog") {
    return;
  }

  const auto variantsStack = hostWindow->findChild<QStackedWidget*>("variantsStack");
  if (!variantsStack || !variantsStack->isAncestorOf(this)) {
    return;
  }

  const QStringList objectNames = {"nameLabel", "modLabel", "previousButton",
                                   "nextButton"};
  for (const auto& objectName : objectNames) {
    if (const auto widget = hostWindow->findChild<QWidget*>(objectName)) {
      m_HostChrome.push_back({widget, widget->isVisible()});
    }
  }

  if (!m_HostChrome.isEmpty()) {
    m_HostWindow = hostWindow;
  }
}

int NifPreviewWidget::secondaryProviderIndex() const
{
  if (m_SourceSet.providers.size() < 2) {
    return m_SourceSet.currentIndex;
  }

  const auto primaryProviderIndex =
      m_LeftPane ? m_LeftPane->currentProviderIndex() : m_SourceSet.currentIndex;
  if (primaryProviderIndex == 0) {
    return 1;
  }

  return 0;
}

void NifPreviewWidget::initializeRightPaneForSplit()
{
  if (m_RightPaneInitialized) {
    return;
  }

  if (const auto leftCamera = m_LeftPane->camera();
      leftCamera && leftCamera->hasState() && m_RightPane->camera()) {
    m_ApplyingCameraSync = true;
    m_RightPane->camera()->setState(leftCamera->state());
    m_ApplyingCameraSync = false;
  }

  m_RightPane->setProviders(m_SourceSet.providers, secondaryProviderIndex());
  m_RightPaneInitialized = true;
  updateCameraSnapshot(m_RightPane);
}

void NifPreviewWidget::handleCameraMoved(NifPreviewPane* pane)
{
  if (!pane || !pane->camera() || !pane->camera()->hasState()) {
    return;
  }

  const auto newState = pane->camera()->state();
  const auto oldState = pane == m_LeftPane ? m_LeftCameraState : m_RightCameraState;
  const auto hasOldState =
      pane == m_LeftPane ? m_HasLeftCameraState : m_HasRightCameraState;

  if (!m_ApplyingCameraSync && hasOldState && m_CameraSyncButton->isChecked() &&
      m_RightPane->isVisible()) {
    syncCameraDelta(pane, oldState, newState);
  }

  updateCameraSnapshot(pane);
}

void NifPreviewWidget::syncCameraDelta(NifPreviewPane* sourcePane,
                                       const CameraState& oldState,
                                       const CameraState& newState)
{
  const auto targetPane = sourcePane == m_LeftPane ? m_RightPane : m_LeftPane;
  if (!targetPane || !targetPane->camera() || !targetPane->camera()->hasState()) {
    return;
  }

  auto targetState = targetPane->camera()->state();
  targetState.lookAt += newState.lookAt - oldState.lookAt;
  targetState.pitch += shortestAngleDelta(oldState.pitch, newState.pitch);
  targetState.yaw += shortestAngleDelta(oldState.yaw, newState.yaw);
  targetState.distance =
      oldState.distance > 0.0f
          ? targetState.distance * (newState.distance / oldState.distance)
          : targetState.distance + (newState.distance - oldState.distance);

  m_ApplyingCameraSync = true;
  targetPane->camera()->setState(targetState);
  m_ApplyingCameraSync = false;
  updateCameraSnapshot(targetPane);
}

void NifPreviewWidget::updateCameraSnapshot(NifPreviewPane* pane)
{
  if (!pane || !pane->camera() || !pane->camera()->hasState()) {
    return;
  }

  if (pane == m_LeftPane) {
    m_LeftCameraState    = pane->camera()->state();
    m_HasLeftCameraState = true;
  } else if (pane == m_RightPane) {
    m_RightCameraState    = pane->camera()->state();
    m_HasRightCameraState = true;
  }
}

void NifPreviewWidget::updateCameraSnapshots()
{
  updateCameraSnapshot(m_LeftPane);
  updateCameraSnapshot(m_RightPane);
}
