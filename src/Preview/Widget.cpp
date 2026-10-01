#include "Preview/Widget.h"
#include "Preview/Pane.h"
#include "Render/Camera.h"

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
constexpr auto kProfileSettingsFileName = "preview_nif.ini";
constexpr auto kProfileSettingsGroup = "Preview";
constexpr auto kSplitPreviewSettingKey = "splitPreview";

QSharedPointer<Camera> MakePreviewCamera() {
    return {new Camera(), &Camera::deleteLater};
}

QString ProfileSettingsPath(const MOBase::IOrganizer* a_organizer) {
    if (!a_organizer || a_organizer->profilePath().isEmpty()) {
        return {};
    }

    return QDir(a_organizer->profilePath()).filePath(kProfileSettingsFileName);
}

bool SplitViewPreference(const MOBase::IOrganizer* a_organizer) {
    const auto settingsPath = ProfileSettingsPath(a_organizer);
    if (settingsPath.isEmpty()) {
        return false;
    }

    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.beginGroup(kProfileSettingsGroup);
    return settings.value(kSplitPreviewSettingKey, false).toBool();
}
} // namespace

NifPreviewWidget::NifPreviewWidget(
    NifPreviewSourceSet a_sourceSet,
    MOBase::IOrganizer* a_organizer,
    QSharedPointer<Camera> a_camera,
    QWidget* a_parent
)
    : QWidget(a_parent)
    , _organizer(a_organizer)
    , _sourceSet(std::move(a_sourceSet)) {
    _globalControlsWidget = new QFrame(this);
    _globalControlsWidget->setObjectName("nifPreviewGlobalToolbar");
    _globalControlsWidget->setFrameShape(QFrame::StyledPanel);
    _globalControlsWidget->setFrameShadow(QFrame::Raised);

    _resetCameraButton = new QPushButton(tr("Reset Camera"), _globalControlsWidget);
    _resetCameraButton->setToolTip(tr("Reset the preview camera"));

    _showCollisionButton = new QCheckBox(tr("Show Collision"), _globalControlsWidget);
    _showCollisionButton->setToolTip(tr("Show collision preview overlay"));

    _showEditorMarkersButton = new QCheckBox(tr("Show Editor Markers"), _globalControlsWidget);
    _showEditorMarkersButton->setToolTip(tr("Show the Creation Kit markers the game hides"));

    _splitButton = new QCheckBox(tr("Split Preview"), _globalControlsWidget);
    _splitButton->setToolTip(tr("Compare two previewable versions of this NIF"));

    _cameraSyncButton = new QCheckBox(tr("Sync Cameras"), _globalControlsWidget);
    _cameraSyncButton->setToolTip(tr("Synchronize cameras between preview panes"));
    _cameraSyncButton->setChecked(true);

    auto* const toolbarLayout = new QHBoxLayout(_globalControlsWidget);
    toolbarLayout->setContentsMargins(8, 4, 8, 4);
    toolbarLayout->setSpacing(12);
    toolbarLayout->addWidget(_resetCameraButton);
    toolbarLayout->addWidget(_showCollisionButton);
    toolbarLayout->addWidget(_showEditorMarkersButton);
    toolbarLayout->addWidget(_splitButton);
    toolbarLayout->addWidget(_cameraSyncButton);
    toolbarLayout->addStretch(1);

    _leftPane = new NifPreviewPane(a_organizer, this);
    _rightPane = new NifPreviewPane(a_organizer, this);

    _leftPane->SetCamera(std::move(a_camera));
    _rightPane->SetCamera(MakePreviewCamera());
    _leftPane->SetProviders(_sourceSet.providers, _sourceSet.currentIndex);
    _rightPane->hide();

    _splitter = new QSplitter(Qt::Horizontal, this);
    _splitter->addWidget(_leftPane);
    _splitter->addWidget(_rightPane);
    _splitter->setChildrenCollapsible(false);

    auto* const rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(4);
    rootLayout->addWidget(_globalControlsWidget);
    rootLayout->addWidget(_splitter, 1);

    connect(_splitButton, &QCheckBox::toggled, this, [this](const bool a_enabled) {
        SetSplitViewEnabled(a_enabled, true);
    });
    connect(_showCollisionButton, &QCheckBox::toggled, this, &NifPreviewWidget::SetShowCollisionEnabled);
    connect(_showEditorMarkersButton, &QCheckBox::toggled, this, &NifPreviewWidget::SetShowEditorMarkersEnabled);
    connect(_cameraSyncButton, &QCheckBox::toggled, this, &NifPreviewWidget::SetCameraSyncEnabled);
    connect(_resetCameraButton, &QPushButton::clicked, this, &NifPreviewWidget::ResetCameras);
    connect(_leftPane, &NifPreviewPane::CameraMoved, this, [this] { HandleCameraMoved(_leftPane); });
    connect(_rightPane, &NifPreviewPane::CameraMoved, this, [this] { HandleCameraMoved(_rightPane); });

    RestoreSplitViewPreference();
    UpdateGlobalControls();
}

NifPreviewWidget::~NifPreviewWidget() {
    _hostChromeGuard.Restore();
}

void NifPreviewWidget::showEvent(QShowEvent* a_event) {
    QWidget::showEvent(a_event);
    QTimer::singleShot(0, this, &NifPreviewWidget::UpdateHostChrome);
}

void NifPreviewWidget::SetSplitViewEnabled(const bool a_enabled, const bool a_persistPreference) {
    if (a_persistPreference) {
        SaveSplitViewPreference(a_enabled);
    }

    if (a_enabled && _sourceSet.providers.size() < 2) {
        const QSignalBlocker blocker(_splitButton);
        _splitButton->setChecked(false);
        return;
    }

    if (a_enabled) {
        InitializeRightPaneForSplit();
    }

    _rightPane->setVisible(a_enabled);
    _cameraSyncButton->setEnabled(a_enabled);

    if (a_enabled && _cameraSyncButton->isChecked()) {
        UpdateCameraSnapshots();
    } else if (!a_enabled) {
        SetCameraSyncEnabled(false);
    }

    UpdateGlobalControls();
    UpdateHostChrome();
}

void NifPreviewWidget::RestoreSplitViewPreference() {
    if (!SplitViewPreference(_organizer)) {
        return;
    }

    const QSignalBlocker blocker(_splitButton);
    _splitButton->setChecked(true);
    SetSplitViewEnabled(true, false);
    _splitButton->setChecked(IsSplitViewEnabled());
}

void NifPreviewWidget::SaveSplitViewPreference(const bool a_enabled) const {
    const auto settingsPath = ProfileSettingsPath(_organizer);
    if (settingsPath.isEmpty()) {
        return;
    }

    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.beginGroup(kProfileSettingsGroup);
    settings.setValue(kSplitPreviewSettingKey, a_enabled);
    settings.sync();
}

bool NifPreviewWidget::IsSplitViewEnabled() const {
    return (_rightPane != nullptr) && !_rightPane->isHidden();
}

bool NifPreviewWidget::IsCameraSyncEnabled() const {
    return IsSplitViewEnabled() && _cameraSyncButton->isChecked();
}

void NifPreviewWidget::SetShowCollisionEnabled(const bool a_enabled) {
    _leftPane->SetShowCollision(a_enabled);
    _rightPane->SetShowCollision(a_enabled);
}

void NifPreviewWidget::SetShowEditorMarkersEnabled(const bool a_enabled) {
    _leftPane->SetShowEditorMarkers(a_enabled);
    _rightPane->SetShowEditorMarkers(a_enabled);
}

void NifPreviewWidget::SetCameraSyncEnabled(const bool a_enabled) {
    if (!IsSplitViewEnabled() && a_enabled) {
        return;
    }

    if (a_enabled) {
        UpdateCameraSnapshots();
    }

    UpdateGlobalControls();
}

void NifPreviewWidget::ResetCameras() {
    _applyingCameraSync = true;
    if (!IsSplitViewEnabled()) {
        _leftPane->ResetCamera();
        _applyingCameraSync = false;
        UpdateCameraSnapshot(_leftPane);
        return;
    }

    _leftPane->ResetCamera();
    _rightPane->ResetCamera();
    _applyingCameraSync = false;
    UpdateCameraSnapshots();
}

void NifPreviewWidget::UpdateGlobalControls() {
    const auto canSplit = _sourceSet.providers.size() > 1;
    _splitButton->setEnabled(canSplit);
    _splitButton->setToolTip(
        canSplit ? tr("Compare two previewable versions of this NIF")
                 : tr("Split view requires at least two previewable "
                      "versions of this NIF")
    );

    const auto splitActive = IsSplitViewEnabled();
    _cameraSyncButton->setVisible(splitActive);
    _cameraSyncButton->setEnabled(splitActive);

    _resetCameraButton->setText(splitActive ? tr("Reset Cameras") : tr("Reset Camera"));
    _resetCameraButton->setToolTip(splitActive ? tr("Reset both preview cameras") : tr("Reset the preview camera"));
}

void NifPreviewWidget::UpdateHostChrome() {
    _hostChromeGuard.HideFor(this);
}

int NifPreviewWidget::SecondaryProviderIndex() const {
    if (_sourceSet.providers.size() < 2) {
        return _sourceSet.currentIndex;
    }

    const auto primaryProviderIndex = _leftPane ? _leftPane->CurrentProviderIndex() : _sourceSet.currentIndex;
    if (primaryProviderIndex == 0) {
        return 1;
    }

    return 0;
}

void NifPreviewWidget::InitializeRightPaneForSplit() {
    if (_rightPaneInitialized) {
        return;
    }

    if (const auto leftCamera = _leftPane->SharedCamera();
        leftCamera && leftCamera->HasState() && _rightPane->SharedCamera()) {
        _applyingCameraSync = true;
        _rightPane->SharedCamera()->SetState(leftCamera->State());
        _applyingCameraSync = false;
    }

    _rightPane->SetProviders(_sourceSet.providers, SecondaryProviderIndex());
    _rightPane->SetShowCollision(_showCollisionButton->isChecked());
    _rightPane->SetShowEditorMarkers(_showEditorMarkersButton->isChecked());
    _rightPaneInitialized = true;
    UpdateCameraSnapshot(_rightPane);
}

void NifPreviewWidget::HandleCameraMoved(NifPreviewPane* a_pane) {
    if (!a_pane || !a_pane->SharedCamera() || !a_pane->SharedCamera()->HasState()) {
        return;
    }

    const auto newState = a_pane->SharedCamera()->State();

    if (!_applyingCameraSync && _cameraSyncButton->isChecked() && IsSplitViewEnabled()) {
        SyncCameraDelta(a_pane, newState);
    }

    UpdateCameraSnapshot(a_pane);
}

void NifPreviewWidget::SyncCameraDelta(NifPreviewPane* a_sourcePane, const CameraState& a_newState) {
    auto* const targetPane = a_sourcePane == _leftPane ? _rightPane : _leftPane;
    if (!targetPane || !targetPane->SharedCamera() || !targetPane->SharedCamera()->HasState()) {
        return;
    }

    const auto targetState = _cameraSynchronizer.SynchronizedTargetState(
        SideForPane(a_sourcePane),
        a_newState,
        targetPane->SharedCamera()->State()
    );
    if (!targetState) {
        return;
    }

    _applyingCameraSync = true;
    targetPane->SharedCamera()->SetState(*targetState);
    _applyingCameraSync = false;
    UpdateCameraSnapshot(targetPane);
}

void NifPreviewWidget::UpdateCameraSnapshot(NifPreviewPane* a_pane) {
    if (!a_pane || !a_pane->SharedCamera() || !a_pane->SharedCamera()->HasState()) {
        return;
    }

    _cameraSynchronizer.UpdateSnapshot(SideForPane(a_pane), a_pane->SharedCamera()->State());
}

void NifPreviewWidget::UpdateCameraSnapshots() {
    UpdateCameraSnapshot(_leftPane);
    UpdateCameraSnapshot(_rightPane);
}

PreviewPaneSide NifPreviewWidget::SideForPane(NifPreviewPane* a_pane) const {
    return a_pane == _leftPane ? PreviewPaneSide::kLeft : PreviewPaneSide::kRight;
}
