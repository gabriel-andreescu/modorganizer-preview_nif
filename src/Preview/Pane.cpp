#include "Preview/Pane.h"
#include "Data/FileHiding.h"
#include "Data/GameFiles.h"
#include "Preview/TexturesPanel.h"
#include "Preview/TexturesSplit.h"
#include "Preview/VersionPicker.h"
#include "Render/NifWidget.h"

#include <QFileInfo>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
QSharedPointer<Camera> MakePaneCamera() {
    return {new Camera(), &Camera::deleteLater};
}

bool SameVersion(const NifPreviewProvider& a_lhs, const NifPreviewProvider& a_rhs) {
    if (a_lhs.kind != a_rhs.kind) {
        return false;
    }
    switch (a_lhs.kind) {
        case NifPreviewProviderKind::kLooseFile:
            return FileHiding::VisiblePath(a_lhs.absolutePath)
                       .compare(FileHiding::VisiblePath(a_rhs.absolutePath), Qt::CaseInsensitive)
                   == 0;
        case NifPreviewProviderKind::kArchive:
            return a_lhs.archivePath.compare(a_rhs.archivePath, Qt::CaseInsensitive) == 0;
        case NifPreviewProviderKind::kInMemory: return true;
    }
    return false;
}
} // namespace

NifPreviewPane::NifPreviewPane(
    MOBase::IOrganizer* a_organizer,
    std::shared_ptr<const GameFiles> a_gameFiles,
    QWidget* a_parent
)
    : QWidget(a_parent)
    , _organizer(a_organizer)
    , _gameFiles(std::move(a_gameFiles))
    , _controller(a_organizer, *_gameFiles)
    , _camera(MakePaneCamera()) {
    _versionPicker = new VersionPicker(this);

    _viewport = new QWidget(this);
    _viewLayout = new QVBoxLayout(_viewport);
    _viewLayout->setContentsMargins(0, 0, 0, 0);

    _texturesPanel = new TexturesPanel(this);
    _statusLabel = new QLabel(this);
    _statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    _texturesSplit = new TexturesSplit(_viewport, _texturesPanel, this);

    auto* const layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_versionPicker);
    layout->addWidget(_texturesSplit, 1);
    layout->addWidget(_statusLabel);

    ConnectControls();
    UpdateTexturesVisibility();
}

void NifPreviewPane::ConnectControls() {
    connect(_versionPicker, &VersionPicker::VersionSelected, this, &NifPreviewPane::SelectProvider);
    connect(_versionPicker, &VersionPicker::HideOthersClicked, this, &NifPreviewPane::HideOtherVersions);
    connect(_versionPicker, &VersionPicker::UnhideClicked, this, &NifPreviewPane::UnhideVersion);

    connect(_texturesPanel, &TexturesPanel::SourceSelected, this, &NifPreviewPane::SelectTextureSource);
    connect(_texturesPanel, &TexturesPanel::PreviewCopy, this, &NifPreviewPane::PreviewTextureCopy);
    connect(
        _texturesPanel,
        &TexturesPanel::HideOthers,
        this,
        [this](const TextureResult& a_result, const qsizetype a_copy) {
            // Once the copy wins, the texture source shows it, so a pick for it would only keep the source as Custom.
            _controller.ClearTexturePick(TextureDataPathKey(a_result.path));
            emit HideOtherCopies(a_result.file.copies, a_copy, QFileInfo(a_result.path).fileName());
        }
    );
    connect(
        _texturesPanel,
        &TexturesPanel::Unhide,
        this,
        [this](const TextureResult& a_result, const qsizetype a_copy) {
            emit UnhideCopy(a_result.file.copies[a_copy], QFileInfo(a_result.path).fileName());
        }
    );
}

void NifPreviewPane::SetProviders(QVector<NifPreviewProvider> a_providers, const int a_currentIndex) {
    _controller.SetProviders(std::move(a_providers), a_currentIndex);
    LoadCurrentProvider();
    ReloadView();
}

void NifPreviewPane::Reload(std::shared_ptr<const GameFiles> a_gameFiles, QVector<NifPreviewProvider> a_providers) {
    const auto selection = _controller.CurrentTextureSelection();
    const auto& providers = _controller.Providers();
    const auto current = _controller.CurrentProviderIndex();
    const auto shown = current < providers.size() ? std::optional(providers[current]) : std::nullopt;

    _gameFiles = std::move(a_gameFiles);
    _controller.SetGameFiles(*_gameFiles);
    auto index = 0;
    if (shown) {
        const auto same = std::ranges::find_if(a_providers, [&](const auto& a_provider) {
            return SameVersion(a_provider, *shown);
        });
        index = same != a_providers.end() ? static_cast<int>(std::distance(a_providers.begin(), same)) : 0;
    }

    _controller.SetProviders(std::move(a_providers), index);
    LoadCurrentProvider();
    _controller.RestoreTextureSelection(selection);
    UpdateTextureSources();
    ReloadView();
}

void NifPreviewPane::SetCamera(QSharedPointer<Camera> a_camera) {
    if (a_camera.isNull()) {
        a_camera = MakePaneCamera();
    }

    if (_cameraConnection) {
        disconnect(_cameraConnection);
    }

    _camera = std::move(a_camera);
    _cameraConnection = connect(_camera.get(), &Camera::CameraMoved, this, &NifPreviewPane::CameraMoved);
    if (_nifWidget) {
        _nifWidget->SetCamera(_camera);
    }
}

void NifPreviewPane::SetShowCollision(const bool a_showCollision) {
    _showCollision = a_showCollision;
    if (_nifWidget) {
        _nifWidget->SetShowCollision(_showCollision);
    }
}

void NifPreviewPane::SetShowEditorMarkers(const bool a_showEditorMarkers) {
    _showEditorMarkers = a_showEditorMarkers;
    if (_nifWidget) {
        _nifWidget->SetShowEditorMarkers(_showEditorMarkers);
    }
}

void NifPreviewPane::SetTexturesOpen(const bool a_open) {
    _texturesOpen = a_open;
    UpdateTexturesVisibility();
    _texturesSplit->FitPanel();
}

// A NIF without textures, such as a skeleton, gets no empty list.
void NifPreviewPane::UpdateTexturesVisibility() {
    const auto hasTextures = !_controller.TextureSources().references.isEmpty() || !TextureResults().isEmpty();
    _texturesPanel->setVisible(_texturesOpen && hasTextures);
}

void NifPreviewPane::SetComparison(std::optional<QVector<TextureResult>> a_other) {
    _texturesPanel->SetComparison(std::move(a_other));
}

void NifPreviewPane::ResetCamera() {
    if (_nifWidget) {
        _nifWidget->ResetCamera();
    }
}

bool NifPreviewPane::TexturesOpen() const {
    return _texturesOpen;
}

QVector<TextureResult> NifPreviewPane::TextureResults() const {
    return _nifWidget ? _nifWidget->TextureResults() : QVector<TextureResult> {};
}

void NifPreviewPane::SelectProvider(const int a_index) {
    if (!_controller.SelectProvider(a_index)) {
        return;
    }

    LoadCurrentProvider();
    // The list would show the previous copy's textures until the new view loads its own.
    _texturesPanel->SetTextures({});
    ReloadView();
}

void NifPreviewPane::SelectTextureSource(const int a_index) {
    if (!_controller.SelectTextureSource(a_index)) {
        return;
    }

    UpdateTextureSources();
    ReloadView();
}

// Choosing the copy the texture source shows anyway drops the texture's pick, so it follows the source again.
void NifPreviewPane::PreviewTextureCopy(const QString& a_key, const QString& a_path) {
    const auto results = TextureResults();
    const auto texture = std::ranges::find_if(results, [&](const TextureResult& a_result) {
        return TextureDataPathKey(a_result.path) == a_key;
    });
    const auto sourceCopy = texture != results.end()
                                ? SourceCopyIndex(_controller.CurrentTextureSelection().source, texture->file.copies)
                                : -1;
    if (sourceCopy >= 0 && texture->file.copies[sourceCopy].path == a_path) {
        _controller.ClearTexturePick(a_key);
    } else {
        _controller.PickTextureCopy(a_key, a_path);
    }

    UpdateTextureSources();
    ReloadView();
}

void NifPreviewPane::HideOtherVersions(const int a_index) {
    const auto& providers = _controller.Providers();
    if (const auto copy = ProviderCopyIndex(providers, a_index); copy >= 0) {
        emit HideOtherCopies(ProviderCopies(providers), copy, QFileInfo(providers[a_index].virtualPath).fileName());
    }
}

void NifPreviewPane::UnhideVersion(const int a_index) {
    const auto& providers = _controller.Providers();
    if (const auto copy = ProviderCopyIndex(providers, a_index); copy >= 0) {
        emit UnhideCopy(ProviderCopies(providers)[copy], QFileInfo(providers[a_index].virtualPath).fileName());
    }
}

void NifPreviewPane::LoadCurrentProvider() {
    const auto result = _controller.LoadCurrentProvider();
    _versionPicker->SetVersions(_controller.Providers(), _controller.CurrentProviderIndex());
    _statusLabel->setText(result.statsText);
    UpdateTextureSources();
    UpdateTexturesVisibility();

    switch (result.status) {
        case PreviewPaneLoadStatus::kNoProvider:
            SetViewWidget(new QLabel(tr("No previewable NIF version"), this));
            return;
        case PreviewPaneLoadStatus::kFailed: SetViewWidget(new QLabel(tr("Failed to load preview"), this)); return;
        case PreviewPaneLoadStatus::kLoaded: return;
    }
}

void NifPreviewPane::ReloadView() {
    const auto nifFile = _controller.CurrentNifFile();
    if (!nifFile) {
        _texturesPanel->SetTextures({});
        return;
    }

    auto* const nifWidget = new NifWidget(
        nifFile,
        _controller.CurrentDataPath(),
        _gameFiles,
        _camera,
        _controller.CurrentTextureSelection(),
        _viewport
    );
    nifWidget->SetShowCollision(_showCollision);
    nifWidget->SetShowEditorMarkers(_showEditorMarkers);
    nifWidget->setMinimumSize(240, 240);
    connect(nifWidget, &NifWidget::TexturesLoaded, this, &NifPreviewPane::HandleTexturesLoaded);
    connect(nifWidget, &NifWidget::AxisViewShown, this, &NifPreviewPane::AxisViewShown);
    SetViewWidget(nifWidget);
}

void NifPreviewPane::UpdateTextureSources() {
    const auto selection = _controller.CurrentTextureSelection();
    _texturesPanel->SetSources(
        _controller.TextureSources(),
        _controller.CurrentTextureSourceIndex(),
        !selection.pickedCopies.isEmpty()
    );
}

void NifPreviewPane::HandleTexturesLoaded() {
    _onlyCollision = _nifWidget != nullptr && !_nifWidget->HasMeshes();
    _texturesPanel->SetTextures(TextureResults());
    UpdateTexturesVisibility();
    _texturesSplit->FitPanel();
    emit TexturesLoaded();
}

void NifPreviewPane::SetViewWidget(QWidget* a_widget) {
    if (_viewWidget) {
        _viewLayout->removeWidget(_viewWidget);
        _viewWidget->deleteLater();
    }

    _viewWidget = a_widget;
    _nifWidget = qobject_cast<NifWidget*>(a_widget);
    if (auto* const label = qobject_cast<QLabel*>(a_widget)) {
        label->setAlignment(Qt::AlignCenter);
        _onlyCollision = false;
        _texturesPanel->SetTextures({});
    }

    _viewLayout->addWidget(a_widget, 1);
}
