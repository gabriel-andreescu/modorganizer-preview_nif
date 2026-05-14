#include "NifPreviewWidget.h"
#include "Camera.h"
#include "NifPreviewPane.h"

#include <QCheckBox>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QSharedPointer>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <uibase/imoinfo.h>
#include <utility>

namespace {
constexpr auto ProfileSettingsFileName = "preview_nif.ini";
constexpr auto ProfileSettingsGroup = "Preview";
constexpr auto SplitPreviewSettingKey = "splitPreview";

QSharedPointer<Camera> makePreviewCamera() {
    return {new Camera(), &Camera::deleteLater};
}

QString profileSettingsPath(MOBase::IOrganizer* organizer) {
    if (!organizer || organizer->profilePath().isEmpty()) {
        return {};
    }

    return QDir(organizer->profilePath()).filePath(ProfileSettingsFileName);
}

bool splitViewPreference(MOBase::IOrganizer* organizer) {
    const auto settingsPath = profileSettingsPath(organizer);
    if (settingsPath.isEmpty()) {
        return false;
    }

    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.beginGroup(ProfileSettingsGroup);
    return settings.value(SplitPreviewSettingKey, false).toBool();
}
} // namespace

NifPreviewWidget::NifPreviewWidget(
    NifPreviewSourceSet sourceSet,
    MOBase::IOrganizer* organizer,
    QSharedPointer<Camera> camera,
    QWidget* parent
)
    : QWidget(parent)
    , m_Organizer(organizer)
    , m_SourceSet(std::move(sourceSet)) {
    m_GlobalControlsWidget = new QFrame(this);
    m_GlobalControlsWidget->setObjectName("nifPreviewGlobalToolbar");
    m_GlobalControlsWidget->setFrameShape(QFrame::StyledPanel);
    m_GlobalControlsWidget->setFrameShadow(QFrame::Raised);

    m_ResetCameraButton = new QPushButton(tr("Reset Camera"), m_GlobalControlsWidget);
    m_ResetCameraButton->setToolTip(tr("Reset the preview camera"));

    m_ShowCollisionButton = new QCheckBox(tr("Show Collision"), m_GlobalControlsWidget);
    m_ShowCollisionButton->setToolTip(tr("Show collision preview overlay"));

    m_SplitButton = new QCheckBox(tr("Split Preview"), m_GlobalControlsWidget);
    m_SplitButton->setToolTip(tr("Compare two previewable versions of this NIF"));

    m_CameraSyncButton = new QCheckBox(tr("Sync Cameras"), m_GlobalControlsWidget);
    m_CameraSyncButton->setToolTip(tr("Synchronize cameras between preview panes"));
    m_CameraSyncButton->setChecked(true);

    auto* const toolbarLayout = new QHBoxLayout(m_GlobalControlsWidget);
    toolbarLayout->setContentsMargins(8, 4, 8, 4);
    toolbarLayout->setSpacing(12);
    toolbarLayout->addWidget(m_ResetCameraButton);
    toolbarLayout->addWidget(m_ShowCollisionButton);
    toolbarLayout->addWidget(m_SplitButton);
    toolbarLayout->addWidget(m_CameraSyncButton);
    toolbarLayout->addStretch(1);

    m_LeftPane = new NifPreviewPane(organizer, this);
    m_RightPane = new NifPreviewPane(organizer, this);

    m_LeftPane->setCamera(std::move(camera));
    m_RightPane->setCamera(makePreviewCamera());
    m_LeftPane->setProviders(m_SourceSet.providers, m_SourceSet.currentIndex);
    m_RightPane->hide();

    m_Splitter = new QSplitter(Qt::Horizontal, this);
    m_Splitter->addWidget(m_LeftPane);
    m_Splitter->addWidget(m_RightPane);
    m_Splitter->setChildrenCollapsible(false);

    auto* const rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(4);
    rootLayout->addWidget(m_GlobalControlsWidget);
    rootLayout->addWidget(m_Splitter, 1);

    connect(m_SplitButton, &QCheckBox::toggled, this, [this](const bool enabled) {
        setSplitViewEnabled(enabled, true);
    });
    connect(m_ShowCollisionButton, &QCheckBox::toggled, this, &NifPreviewWidget::setShowCollisionEnabled);
    connect(m_CameraSyncButton, &QCheckBox::toggled, this, &NifPreviewWidget::setCameraSyncEnabled);
    connect(m_ResetCameraButton, &QPushButton::clicked, this, &NifPreviewWidget::resetCameras);
    connect(m_LeftPane, &NifPreviewPane::cameraMoved, this, [this]() {
        handleCameraMoved(m_LeftPane);
    });
    connect(m_RightPane, &NifPreviewPane::cameraMoved, this, [this]() {
        handleCameraMoved(m_RightPane);
    });

    restoreSplitViewPreference();
    updateGlobalControls();
}

NifPreviewWidget::~NifPreviewWidget() {
    m_HostChromeGuard.restore();
}

void NifPreviewWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &NifPreviewWidget::updateHostChrome);
}

void NifPreviewWidget::setSplitViewEnabled(const bool enabled, const bool persistPreference) {
    if (persistPreference) {
        saveSplitViewPreference(enabled);
    }

    if (enabled && m_SourceSet.providers.size() < 2) {
        const QSignalBlocker blocker(m_SplitButton);
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

void NifPreviewWidget::restoreSplitViewPreference() {
    if (!splitViewPreference(m_Organizer)) {
        return;
    }

    const QSignalBlocker blocker(m_SplitButton);
    m_SplitButton->setChecked(true);
    setSplitViewEnabled(true, false);
    m_SplitButton->setChecked(isSplitViewEnabled());
}

void NifPreviewWidget::saveSplitViewPreference(const bool enabled) const {
    const auto settingsPath = profileSettingsPath(m_Organizer);
    if (settingsPath.isEmpty()) {
        return;
    }

    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.beginGroup(ProfileSettingsGroup);
    settings.setValue(SplitPreviewSettingKey, enabled);
    settings.sync();
}

bool NifPreviewWidget::isSplitViewEnabled() const {
    return m_RightPane && !m_RightPane->isHidden();
}

void NifPreviewWidget::setShowCollisionEnabled(const bool enabled) {
    m_LeftPane->setShowCollision(enabled);
    m_RightPane->setShowCollision(enabled);
}

void NifPreviewWidget::setCameraSyncEnabled(const bool enabled) {
    if (!isSplitViewEnabled() && enabled) {
        return;
    }

    if (enabled) {
        updateCameraSnapshots();
    }

    updateGlobalControls();
}

void NifPreviewWidget::resetCameras() {
    m_ApplyingCameraSync = true;
    if (!isSplitViewEnabled()) {
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

void NifPreviewWidget::updateGlobalControls() {
    const auto canSplit = m_SourceSet.providers.size() > 1;
    m_SplitButton->setEnabled(canSplit);
    m_SplitButton->setToolTip(
        canSplit ? tr("Compare two previewable versions of this NIF")
                 : tr("Split view requires at least two previewable "
                      "versions of this NIF")
    );

    const auto splitActive = isSplitViewEnabled();
    m_CameraSyncButton->setVisible(splitActive);
    m_CameraSyncButton->setEnabled(splitActive);

    m_ResetCameraButton->setText(splitActive ? tr("Reset Cameras") : tr("Reset Camera"));
    m_ResetCameraButton->setToolTip(splitActive ? tr("Reset both preview cameras") : tr("Reset the preview camera"));
}

void NifPreviewWidget::updateHostChrome() {
    m_HostChromeGuard.hideFor(this);
}

int NifPreviewWidget::secondaryProviderIndex() const {
    if (m_SourceSet.providers.size() < 2) {
        return m_SourceSet.currentIndex;
    }

    const auto primaryProviderIndex = m_LeftPane ? m_LeftPane->currentProviderIndex() : m_SourceSet.currentIndex;
    if (primaryProviderIndex == 0) {
        return 1;
    }

    return 0;
}

void NifPreviewWidget::initializeRightPaneForSplit() {
    if (m_RightPaneInitialized) {
        return;
    }

    if (const auto leftCamera = m_LeftPane->camera(); leftCamera && leftCamera->hasState() && m_RightPane->camera()) {
        m_ApplyingCameraSync = true;
        m_RightPane->camera()->setState(leftCamera->state());
        m_ApplyingCameraSync = false;
    }

    m_RightPane->setProviders(m_SourceSet.providers, secondaryProviderIndex());
    m_RightPane->setShowCollision(m_ShowCollisionButton->isChecked());
    m_RightPaneInitialized = true;
    updateCameraSnapshot(m_RightPane);
}

void NifPreviewWidget::handleCameraMoved(NifPreviewPane* pane) {
    if (!pane || !pane->camera() || !pane->camera()->hasState()) {
        return;
    }

    const auto newState = pane->camera()->state();

    if (!m_ApplyingCameraSync && m_CameraSyncButton->isChecked() && isSplitViewEnabled()) {
        syncCameraDelta(pane, newState);
    }

    updateCameraSnapshot(pane);
}

void NifPreviewWidget::syncCameraDelta(NifPreviewPane* sourcePane, const CameraState& newState) {
    auto* const targetPane = sourcePane == m_LeftPane ? m_RightPane : m_LeftPane;
    if (!targetPane || !targetPane->camera() || !targetPane->camera()->hasState()) {
        return;
    }

    const auto targetState = m_CameraSynchronizer.synchronizedTargetState(
        sideForPane(sourcePane),
        newState,
        targetPane->camera()->state()
    );
    if (!targetState) {
        return;
    }

    m_ApplyingCameraSync = true;
    targetPane->camera()->setState(*targetState);
    m_ApplyingCameraSync = false;
    updateCameraSnapshot(targetPane);
}

void NifPreviewWidget::updateCameraSnapshot(NifPreviewPane* pane) {
    if (!pane || !pane->camera() || !pane->camera()->hasState()) {
        return;
    }

    m_CameraSynchronizer.updateSnapshot(sideForPane(pane), pane->camera()->state());
}

void NifPreviewWidget::updateCameraSnapshots() {
    updateCameraSnapshot(m_LeftPane);
    updateCameraSnapshot(m_RightPane);
}

PreviewPaneSide NifPreviewWidget::sideForPane(NifPreviewPane* pane) const {
    return pane == m_LeftPane ? PreviewPaneSide::Left : PreviewPaneSide::Right;
}
