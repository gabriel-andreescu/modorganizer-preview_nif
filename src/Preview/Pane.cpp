#include "Preview/Pane.h"
#include "Render/NifWidget.h"

#include <QComboBox>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
constexpr int kMinSourceComboWidth = 180;
constexpr int kMinSourceComboMaxWidth = 220;
constexpr int kMaxSourceComboWidth = 360;
constexpr int kSourceComboChromeWidth = 48;

QSharedPointer<Camera> MakePaneCamera() {
    return {new Camera(), &Camera::deleteLater};
}

QLabel* MakeSelectableLabel(QWidget* a_parent) {
    auto* const label = new QLabel(a_parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QToolButton* MakeArrowButton(QWidget* a_parent, const Qt::ArrowType a_arrow, const QString& a_toolTip) {
    auto* const button = new QToolButton(a_parent);
    button->setArrowType(a_arrow);
    button->setToolTip(a_toolTip);
    return button;
}

QComboBox* MakeSourceCombo(QWidget* a_parent) {
    auto* const combo = new QComboBox(a_parent);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(12);
    combo->setMinimumWidth(kMinSourceComboWidth);
    combo->setMaximumWidth(kMinSourceComboMaxWidth);
    combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return combo;
}
} // namespace

NifPreviewPane::NifPreviewPane(MOBase::IOrganizer* a_organizer, QWidget* a_parent)
    : QWidget(a_parent)
    , _organizer(a_organizer)
    , _controller(a_organizer)
    , _camera(MakePaneCamera()) {
    CreateControls();
    CreateLayout();
    ConnectControls();
    UpdateTextureControls();
}

void NifPreviewPane::CreateControls() {
    _titleLabel = MakeSelectableLabel(this);
    _prevButton = MakeArrowButton(this, Qt::LeftArrow, tr("Previous version"));
    _sourceCombo = MakeSourceCombo(this);
    _nextButton = MakeArrowButton(this, Qt::RightArrow, tr("Next version"));

    _textureLabel = MakeSelectableLabel(this);
    _prevTextureButton = MakeArrowButton(this, Qt::LeftArrow, tr("Previous texture source"));
    _textureSourceCombo = MakeSourceCombo(this);
    _nextTextureButton = MakeArrowButton(this, Qt::RightArrow, tr("Next texture source"));

    _statsLabel = MakeSelectableLabel(this);
    _statsLabel->setWordWrap(true);
}

void NifPreviewPane::CreateLayout() {
    auto* const headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(_titleLabel, 1);
    headerLayout->addWidget(_prevButton);
    headerLayout->addWidget(_sourceCombo);
    headerLayout->addWidget(_nextButton);

    auto* const textureLayout = new QHBoxLayout();
    textureLayout->setContentsMargins(0, 0, 0, 0);
    textureLayout->addWidget(_textureLabel, 1);
    textureLayout->addWidget(_prevTextureButton);
    textureLayout->addWidget(_textureSourceCombo);
    textureLayout->addWidget(_nextTextureButton);

    auto* const viewFrame = new QFrame(this);
    viewFrame->setFrameShape(QFrame::NoFrame);
    _viewLayout = new QVBoxLayout(viewFrame);
    _viewLayout->setContentsMargins(0, 0, 0, 0);

    auto* const rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addLayout(headerLayout);
    rootLayout->addLayout(textureLayout);
    rootLayout->addWidget(viewFrame, 1);
    rootLayout->addWidget(_statsLabel);
}

void NifPreviewPane::ConnectControls() {
    connect(_prevButton, &QToolButton::clicked, this, [this] { SelectRelativeProvider(1); });
    connect(_nextButton, &QToolButton::clicked, this, [this] { SelectRelativeProvider(-1); });
    connect(_sourceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int a_index) {
        if (!_updatingControls) {
            SelectProvider(a_index);
        }
    });
    connect(_prevTextureButton, &QToolButton::clicked, this, [this] { SelectRelativeTextureSource(1); });
    connect(_nextTextureButton, &QToolButton::clicked, this, [this] { SelectRelativeTextureSource(-1); });
    connect(_textureSourceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int a_index) {
        if (!_updatingTextureControls) {
            SelectTextureSource(a_index);
        }
    });
}

void NifPreviewPane::resizeEvent(QResizeEvent* a_event) {
    QWidget::resizeEvent(a_event);
    UpdateSourceComboWidth();
    UpdateTextureSourceComboWidth();
}

void NifPreviewPane::SetProviders(QVector<NifPreviewProvider> a_providers, const int a_currentIndex) {
    _controller.SetProviders(std::move(a_providers), a_currentIndex);

    _updatingControls = true;
    _sourceCombo->clear();
    for (const auto& provider : _controller.Providers()) {
        _sourceCombo->addItem(provider.displayName);
        _sourceCombo->setItemData(_sourceCombo->count() - 1, provider.displayName, Qt::ToolTipRole);
    }
    _sourceCombo->setCurrentIndex(_controller.CurrentProviderIndex());
    _updatingControls = false;

    UpdateSourceComboWidth();
    UpdateControls();
    LoadCurrentProvider();
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
    if (_showCollision == a_showCollision) {
        return;
    }

    _showCollision = a_showCollision;
    if (_nifWidget) {
        _nifWidget->SetShowCollision(_showCollision);
    }
}

void NifPreviewPane::SetShowEditorMarkers(const bool a_showEditorMarkers) {
    if (_showEditorMarkers == a_showEditorMarkers) {
        return;
    }

    _showEditorMarkers = a_showEditorMarkers;
    if (_nifWidget) {
        _nifWidget->SetShowEditorMarkers(_showEditorMarkers);
    }
}

void NifPreviewPane::ResetCamera() {
    if (_nifWidget) {
        _nifWidget->ResetCamera();
    }
}

void NifPreviewPane::SelectProvider(const int a_index) {
    if (!_controller.SelectProvider(a_index)) {
        return;
    }

    UpdateControls();
    LoadCurrentProvider();
    emit ProviderChanged(_controller.CurrentProviderIndex());
}

void NifPreviewPane::SelectRelativeProvider(const int a_offset) {
    if (!_controller.SelectRelativeProvider(a_offset)) {
        return;
    }

    UpdateControls();
    LoadCurrentProvider();
    emit ProviderChanged(_controller.CurrentProviderIndex());
}

void NifPreviewPane::SelectTextureSource(const int a_index) {
    if (!_controller.SelectTextureSource(a_index)) {
        return;
    }

    UpdateTextureControls();
    ReloadCurrentNifWidget();
}

void NifPreviewPane::SelectRelativeTextureSource(const int a_offset) {
    if (!_controller.SelectRelativeTextureSource(a_offset)) {
        return;
    }

    UpdateTextureControls();
    ReloadCurrentNifWidget();
}

void NifPreviewPane::UpdateControls() {
    const auto hasMultipleProviders = _controller.Providers().size() > 1;
    _prevButton->setEnabled(hasMultipleProviders);
    _nextButton->setEnabled(hasMultipleProviders);
    _sourceCombo->setEnabled(hasMultipleProviders);

    _updatingControls = true;
    _sourceCombo->setCurrentIndex(_controller.CurrentProviderIndex());
    _updatingControls = false;
}

void NifPreviewPane::UpdateSourceComboWidth() {
    if (!_sourceCombo) {
        return;
    }

    const QFontMetrics metrics(_sourceCombo->font());
    int widestProvider = 0;
    for (const auto& provider : _controller.Providers()) {
        widestProvider = std::max(widestProvider, metrics.horizontalAdvance(provider.displayName));
    }

    const auto paneLimitedMax = std::clamp(width() / 3, kMinSourceComboMaxWidth, kMaxSourceComboWidth);
    const auto desiredWidth = std::clamp(
        widestProvider + kSourceComboChromeWidth,
        kMinSourceComboWidth,
        paneLimitedMax
    );

    _sourceCombo->setMinimumWidth(desiredWidth);
    _sourceCombo->setMaximumWidth(desiredWidth);
}

void NifPreviewPane::UpdateTextureSourceComboItems() {
    _updatingTextureControls = true;
    _textureSourceCombo->clear();
    for (const auto& provider : _controller.TextureSources().providers) {
        _textureSourceCombo->addItem(provider.displayName);
        _textureSourceCombo->setItemData(_textureSourceCombo->count() - 1, provider.displayName, Qt::ToolTipRole);
    }
    _textureSourceCombo->setCurrentIndex(_controller.CurrentTextureSourceIndex());
    _updatingTextureControls = false;

    UpdateTextureSourceComboWidth();
    UpdateTextureControls();
}

void NifPreviewPane::UpdateTextureControls() {
    const auto& textureSources = _controller.TextureSources();
    const auto currentTextureSourceIndex = _controller.CurrentTextureSourceIndex();
    const auto hasMultipleTextureSources = textureSources.providers.size() > 2;

    _prevTextureButton->setEnabled(hasMultipleTextureSources);
    _nextTextureButton->setEnabled(hasMultipleTextureSources);
    _textureSourceCombo->setEnabled(hasMultipleTextureSources);

    const auto toolTip = MakeTextureToolTipText(textureSources, currentTextureSourceIndex);
    _textureLabel->setText(MakeTextureSummaryText(textureSources));
    _textureLabel->setToolTip(toolTip);
    _textureSourceCombo->setToolTip(toolTip);

    _updatingTextureControls = true;
    _textureSourceCombo->setCurrentIndex(currentTextureSourceIndex);
    _updatingTextureControls = false;
}

void NifPreviewPane::UpdateTextureSourceComboWidth() {
    if (!_textureSourceCombo) {
        return;
    }

    const QFontMetrics metrics(_textureSourceCombo->font());
    int widestProvider = 0;
    for (const auto& provider : _controller.TextureSources().providers) {
        widestProvider = std::max(widestProvider, metrics.horizontalAdvance(provider.displayName));
    }

    const auto paneLimitedMax = std::clamp(width() / 3, kMinSourceComboMaxWidth, kMaxSourceComboWidth);
    const auto desiredWidth = std::clamp(
        widestProvider + kSourceComboChromeWidth,
        kMinSourceComboWidth,
        paneLimitedMax
    );

    _textureSourceCombo->setMinimumWidth(desiredWidth);
    _textureSourceCombo->setMaximumWidth(desiredWidth);
}

void NifPreviewPane::LoadCurrentProvider() {
    const auto result = _controller.LoadCurrentProvider();
    _titleLabel->setText(result.title);
    _statsLabel->setText(result.statsText);
    UpdateTextureSourceComboItems();

    switch (result.status) {
        case PreviewPaneLoadStatus::kNoProvider:
            _titleLabel->clear();
            SetViewWidget(new QLabel(tr("No previewable NIF version"), this));
            return;
        case PreviewPaneLoadStatus::kFailed: SetViewWidget(new QLabel(tr("Failed to load preview"), this)); return;
        case PreviewPaneLoadStatus::kLoaded: ReloadCurrentNifWidget(); return;
    }
}

void NifPreviewPane::ReloadCurrentNifWidget() {
    const auto nifFile = _controller.CurrentNifFile();
    if (!nifFile) {
        return;
    }

    auto* const nifWidget = new NifWidget(
        nifFile,
        _controller.CurrentDataPath(),
        _organizer,
        _camera,
        _controller.CurrentTextureSourceProvider(),
        this
    );
    nifWidget->SetShowCollision(_showCollision);
    nifWidget->SetShowEditorMarkers(_showEditorMarkers);
    nifWidget->setMinimumSize(240, 240);
    _nifWidget = nifWidget;
    SetViewWidget(nifWidget);
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
    }

    _viewLayout->addWidget(a_widget, 1);
}
