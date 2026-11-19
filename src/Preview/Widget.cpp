#include "Preview/Widget.h"
#include "Preview/CopyActions.h"
#include "Preview/NoticeBar.h"
#include "Preview/Pane.h"
#include "Preview/ViewToolbar.h"

#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace {
QSharedPointer<Camera> MakePreviewCamera() {
    return {new Camera(), &Camera::deleteLater};
}
} // namespace

NifPreviewWidget::NifPreviewWidget(
    NifPreviewResolution a_resolution,
    NifPreviewResolver a_resolver,
    MOBase::IOrganizer& a_organizer,
    QSharedPointer<Camera> a_camera,
    QWidget* a_parent
)
    : QWidget(a_parent)
    , _resolver(std::move(a_resolver))
    , _preferences(a_organizer)
    , _gameFiles(std::move(a_resolution.gameFiles))
    , _sourceSet(std::move(a_resolution.sourceSet)) {
    _toolbar = new ViewToolbar(this);
    _notice = new NoticeBar(this);
    _copyActions = new CopyActions(a_organizer, this);

    const auto showTextures = _preferences.ShowTextures();
    _toolbar->SetShowTextures(showTextures);
    _leftPane = new NifPreviewPane(&a_organizer, _gameFiles, this);
    _rightPane = new NifPreviewPane(&a_organizer, _gameFiles, this);
    _leftPane->SetCamera(std::move(a_camera));
    _rightPane->SetCamera(MakePreviewCamera());
    _leftPane->SetProviders(_sourceSet.providers, _sourceSet.currentIndex);
    _leftPane->SetTexturesOpen(showTextures);
    _rightPane->SetTexturesOpen(showTextures);
    _rightPane->hide();

    _splitter = new QSplitter(Qt::Horizontal, this);
    _splitter->addWidget(_leftPane);
    _splitter->addWidget(_rightPane);
    _splitter->setChildrenCollapsible(false);

    auto* const layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(_toolbar);
    layout->addWidget(_notice);
    layout->addWidget(_splitter, 1);

    ConnectToolbar();
    ConnectPane(_leftPane);
    ConnectPane(_rightPane);
    ConnectCopyActions();

    UpdateSplitAvailability();
    if (_preferences.SplitPreview()) {
        SetSplitEnabled(true);
    }
}

NifPreviewWidget::~NifPreviewWidget() {
    _hostChromeGuard.Restore();
}

void NifPreviewWidget::showEvent(QShowEvent* a_event) {
    QWidget::showEvent(a_event);
    QTimer::singleShot(0, this, [this] { _hostChromeGuard.HideFor(this); });
}

void NifPreviewWidget::ConnectToolbar() {
    connect(_toolbar, &ViewToolbar::ResetCameraClicked, this, &NifPreviewWidget::ResetCameras);
    connect(_toolbar, &ViewToolbar::CollisionToggled, this, &NifPreviewWidget::SetShowCollision);
    connect(_toolbar, &ViewToolbar::EditorMarkersToggled, this, &NifPreviewWidget::SetShowEditorMarkers);
    connect(_toolbar, &ViewToolbar::TexturesToggled, this, [this](const bool a_show) {
        _preferences.SetShowTextures(a_show);
        SetShowTextures(a_show);
    });
    connect(_toolbar, &ViewToolbar::SplitToggled, this, [this](const bool a_enabled) {
        _preferences.SetSplitPreview(a_enabled);
        SetSplitEnabled(a_enabled);
    });
    connect(_toolbar, &ViewToolbar::CameraSyncToggled, this, &NifPreviewWidget::SetCameraSyncEnabled);
}

void NifPreviewWidget::ConnectPane(NifPreviewPane* a_pane) {
    connect(a_pane, &NifPreviewPane::CameraMoved, this, [this, a_pane] { HandleCameraMoved(a_pane); });
    connect(a_pane, &NifPreviewPane::TexturesLoaded, this, &NifPreviewWidget::HandleTexturesLoaded);
    connect(a_pane, &NifPreviewPane::HideOtherCopies, _copyActions, &CopyActions::MakeWinner);
    connect(a_pane, &NifPreviewPane::UnhideCopy, _copyActions, &CopyActions::Restore);
}

void NifPreviewWidget::ConnectCopyActions() {
    connect(_copyActions, &CopyActions::Waiting, this, [this](const QString& a_message) {
        _notice->Show(a_message, false);
    });
    connect(_copyActions, &CopyActions::Applied, this, [this](const QString& a_message, const bool a_canUndo) {
        Refresh();
        _notice->Show(a_message, a_canUndo);
    });
    connect(_copyActions, &CopyActions::Failed, this, [this](const QString& a_message) {
        _notice->Show(a_message, false);
    });
    connect(_notice, &NoticeBar::UndoClicked, _copyActions, &CopyActions::Undo);
}

void NifPreviewWidget::SetSplitEnabled(const bool a_enabled) {
    if (a_enabled && _sourceSet.providers.size() < 2) {
        _toolbar->SetSplit(false);
        return;
    }

    _toolbar->SetSplit(a_enabled);
    if (a_enabled) {
        InitializeRightPane();
    }
    _rightPane->setVisible(a_enabled);

    if (a_enabled && _toolbar->SyncsCameras()) {
        UpdateCameraSnapshots();
    }
    UpdateComparison();
    UpdateCollisionLock();
    _hostChromeGuard.HideFor(this);
}

bool NifPreviewWidget::IsSplitViewEnabled() const {
    return !_rightPane->isHidden();
}

bool NifPreviewWidget::IsCameraSyncEnabled() const {
    return IsSplitViewEnabled() && _toolbar->SyncsCameras();
}

NifPreviewWidget::ViewOptions NifPreviewWidget::Options() const {
    return {.collision = _leftPane->ShowsCollision(), .editorMarkers = _leftPane->ShowsEditorMarkers()};
}

void NifPreviewWidget::SetOptions(const ViewOptions& a_options) {
    _toolbar->SetShowCollision(a_options.collision);
    _toolbar->SetShowEditorMarkers(a_options.editorMarkers);
    SetShowCollision(a_options.collision);
    SetShowEditorMarkers(a_options.editorMarkers);
}

void NifPreviewWidget::SetShowCollision(const bool a_show) {
    _leftPane->SetShowCollision(a_show);
    _rightPane->SetShowCollision(a_show);
}

void NifPreviewWidget::SetShowEditorMarkers(const bool a_show) {
    _leftPane->SetShowEditorMarkers(a_show);
    _rightPane->SetShowEditorMarkers(a_show);
}

void NifPreviewWidget::SetShowTextures(const bool a_show) {
    _leftPane->SetTexturesOpen(a_show);
    _rightPane->SetTexturesOpen(a_show);
}

void NifPreviewWidget::SetCameraSyncEnabled(const bool a_enabled) {
    if (a_enabled && IsSplitViewEnabled()) {
        UpdateCameraSnapshots();
    }
}

void NifPreviewWidget::ResetCameras() {
    _leftPane->ResetCamera();
    if (IsSplitViewEnabled()) {
        _rightPane->ResetCamera();
    }
}

void NifPreviewWidget::HandleTexturesLoaded() {
    UpdateComparison();
    UpdateCollisionLock();
}

void NifPreviewWidget::UpdateComparison() {
    if (!IsSplitViewEnabled()) {
        _leftPane->SetComparison(std::nullopt);
        return;
    }

    _leftPane->SetComparison(_rightPane->TextureResults());
    _rightPane->SetComparison(_leftPane->TextureResults());
}

void NifPreviewWidget::UpdateCollisionLock() {
    _toolbar->SetCollisionLocked(
        _leftPane->ShowsOnlyCollision() && (!IsSplitViewEnabled() || _rightPane->ShowsOnlyCollision())
    );
}

void NifPreviewWidget::UpdateSplitAvailability() {
    _toolbar->SetSplitUnavailableReason(
        _sourceSet.providers.size() < 2 ? tr("Split Preview needs a second copy of this mesh") : QString()
    );
}

// MO2 finished refreshing after the preview hid or restored files, so the versions and textures resolve again.
void NifPreviewWidget::Refresh() {
    auto [gameFiles, sourceSet] = _resolver();
    _gameFiles = std::move(gameFiles);
    _sourceSet = std::move(sourceSet);
    _leftPane->Reload(_gameFiles, _sourceSet.providers);
    if (_rightPaneInitialized) {
        _rightPane->Reload(_gameFiles, _sourceSet.providers);
    }
    UpdateSplitAvailability();
}

int NifPreviewWidget::SecondaryProviderIndex() const {
    return _leftPane->CurrentProviderIndex() == 0 ? 1 : 0;
}

void NifPreviewWidget::InitializeRightPane() {
    if (_rightPaneInitialized) {
        return;
    }

    if (const auto leftCamera = _leftPane->SharedCamera(); leftCamera && leftCamera->HasState()) {
        _applyingCameraSync = true;
        _rightPane->SharedCamera()->SetState(leftCamera->State());
        _applyingCameraSync = false;
    }

    _rightPane->SetProviders(_sourceSet.providers, SecondaryProviderIndex());
    _rightPane->SetShowCollision(_leftPane->ShowsCollision());
    _rightPane->SetShowEditorMarkers(_leftPane->ShowsEditorMarkers());
    _rightPaneInitialized = true;
    UpdateCameraSnapshot(_rightPane);
}

void NifPreviewWidget::HandleCameraMoved(NifPreviewPane* a_pane) {
    if (!a_pane->SharedCamera()->HasState()) {
        return;
    }

    const auto newState = a_pane->SharedCamera()->State();
    if (!_applyingCameraSync && IsCameraSyncEnabled()) {
        SyncCameraDelta(a_pane, newState);
    }
    UpdateCameraSnapshot(a_pane);
}

void NifPreviewWidget::SyncCameraDelta(NifPreviewPane* a_sourcePane, const CameraState& a_newState) {
    auto* const targetPane = a_sourcePane == _leftPane ? _rightPane : _leftPane;
    if (!targetPane->SharedCamera()->HasState()) {
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
    if (!a_pane->SharedCamera()->HasState()) {
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
