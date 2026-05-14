#include "NifPreviewPane.h"
#include "NifWidget.h"

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
constexpr int minSourceComboWidth = 180;
constexpr int minSourceComboMaxWidth = 220;
constexpr int maxSourceComboWidth = 360;
constexpr int sourceComboChromeWidth = 48;

QSharedPointer<Camera> makePaneCamera() {
    return {new Camera(), &Camera::deleteLater};
}
} // namespace

NifPreviewPane::NifPreviewPane(MOBase::IOrganizer* organizer, QWidget* parent)
    : QWidget(parent)
    , m_Organizer(organizer)
    , m_Controller(organizer)
    , m_Camera(makePaneCamera()) {
    m_TitleLabel = new QLabel(this);
    m_TitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_PrevButton = new QToolButton(this);
    m_PrevButton->setArrowType(Qt::LeftArrow);
    m_PrevButton->setToolTip(tr("Previous version"));

    m_SourceCombo = new QComboBox(this);
    m_SourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_SourceCombo->setMinimumContentsLength(12);
    m_SourceCombo->setMinimumWidth(minSourceComboWidth);
    m_SourceCombo->setMaximumWidth(minSourceComboMaxWidth);
    m_SourceCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_NextButton = new QToolButton(this);
    m_NextButton->setArrowType(Qt::RightArrow);
    m_NextButton->setToolTip(tr("Next version"));

    m_TextureLabel = new QLabel(this);
    m_TextureLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_PrevTextureButton = new QToolButton(this);
    m_PrevTextureButton->setArrowType(Qt::LeftArrow);
    m_PrevTextureButton->setToolTip(tr("Previous texture source"));

    m_TextureSourceCombo = new QComboBox(this);
    m_TextureSourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_TextureSourceCombo->setMinimumContentsLength(12);
    m_TextureSourceCombo->setMinimumWidth(minSourceComboWidth);
    m_TextureSourceCombo->setMaximumWidth(minSourceComboMaxWidth);
    m_TextureSourceCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_NextTextureButton = new QToolButton(this);
    m_NextTextureButton->setArrowType(Qt::RightArrow);
    m_NextTextureButton->setToolTip(tr("Next texture source"));

    m_StatsLabel = new QLabel(this);
    m_StatsLabel->setWordWrap(true);
    m_StatsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* const headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(m_TitleLabel, 1);
    headerLayout->addWidget(m_PrevButton);
    headerLayout->addWidget(m_SourceCombo);
    headerLayout->addWidget(m_NextButton);

    auto* const textureLayout = new QHBoxLayout();
    textureLayout->setContentsMargins(0, 0, 0, 0);
    textureLayout->addWidget(m_TextureLabel, 1);
    textureLayout->addWidget(m_PrevTextureButton);
    textureLayout->addWidget(m_TextureSourceCombo);
    textureLayout->addWidget(m_NextTextureButton);

    auto* const viewFrame = new QFrame(this);
    viewFrame->setFrameShape(QFrame::NoFrame);
    m_ViewLayout = new QVBoxLayout(viewFrame);
    m_ViewLayout->setContentsMargins(0, 0, 0, 0);

    auto* const rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addLayout(headerLayout);
    rootLayout->addLayout(textureLayout);
    rootLayout->addWidget(viewFrame, 1);
    rootLayout->addWidget(m_StatsLabel);

    connect(m_PrevButton, &QToolButton::clicked, this, [this]() {
        selectRelativeProvider(1);
    });
    connect(m_NextButton, &QToolButton::clicked, this, [this]() {
        selectRelativeProvider(-1);
    });
    connect(m_SourceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        if (!m_UpdatingControls) {
            selectProvider(index);
        }
    });
    connect(m_PrevTextureButton, &QToolButton::clicked, this, [this]() {
        selectRelativeTextureSource(1);
    });
    connect(m_NextTextureButton, &QToolButton::clicked, this, [this]() {
        selectRelativeTextureSource(-1);
    });
    connect(m_TextureSourceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int index) {
        if (!m_UpdatingTextureControls) {
            selectTextureSource(index);
        }
    });

    updateTextureControls();
}

void NifPreviewPane::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateSourceComboWidth();
    updateTextureSourceComboWidth();
}

void NifPreviewPane::setProviders(QVector<NifPreviewProvider> providers, const int currentIndex) {
    m_Controller.setProviders(std::move(providers), currentIndex);

    m_UpdatingControls = true;
    m_SourceCombo->clear();
    for (const auto& provider : m_Controller.providers()) {
        m_SourceCombo->addItem(provider.displayName);
        m_SourceCombo->setItemData(m_SourceCombo->count() - 1, provider.displayName, Qt::ToolTipRole);
    }
    m_SourceCombo->setCurrentIndex(m_Controller.currentProviderIndex());
    m_UpdatingControls = false;

    updateSourceComboWidth();
    updateControls();
    loadCurrentProvider();
}

void NifPreviewPane::setCamera(QSharedPointer<Camera> camera) {
    if (camera.isNull()) {
        camera = makePaneCamera();
    }

    if (m_CameraConnection) {
        disconnect(m_CameraConnection);
    }

    m_Camera = std::move(camera);
    m_CameraConnection = connect(m_Camera.get(), &Camera::cameraMoved, this, &NifPreviewPane::cameraMoved);
    if (m_NifWidget) {
        m_NifWidget->setCamera(m_Camera);
    }
}

void NifPreviewPane::setShowCollision(const bool showCollision) {
    if (m_ShowCollision == showCollision) {
        return;
    }

    m_ShowCollision = showCollision;
    if (m_NifWidget) {
        m_NifWidget->setShowCollision(m_ShowCollision);
    }
}

void NifPreviewPane::resetCamera() {
    if (m_NifWidget) {
        m_NifWidget->resetCamera();
    }
}

void NifPreviewPane::selectProvider(const int index) {
    if (!m_Controller.selectProvider(index)) {
        return;
    }

    updateControls();
    loadCurrentProvider();
    emit providerChanged(m_Controller.currentProviderIndex());
}

void NifPreviewPane::selectRelativeProvider(const int offset) {
    if (!m_Controller.selectRelativeProvider(offset)) {
        return;
    }

    updateControls();
    loadCurrentProvider();
    emit providerChanged(m_Controller.currentProviderIndex());
}

void NifPreviewPane::selectTextureSource(const int index) {
    if (!m_Controller.selectTextureSource(index)) {
        return;
    }

    updateTextureControls();
    reloadCurrentNifWidget();
}

void NifPreviewPane::selectRelativeTextureSource(const int offset) {
    if (!m_Controller.selectRelativeTextureSource(offset)) {
        return;
    }

    updateTextureControls();
    reloadCurrentNifWidget();
}

void NifPreviewPane::updateControls() {
    const auto hasMultipleProviders = m_Controller.providers().size() > 1;
    m_PrevButton->setEnabled(hasMultipleProviders);
    m_NextButton->setEnabled(hasMultipleProviders);
    m_SourceCombo->setEnabled(hasMultipleProviders);

    m_UpdatingControls = true;
    m_SourceCombo->setCurrentIndex(m_Controller.currentProviderIndex());
    m_UpdatingControls = false;
}

void NifPreviewPane::updateSourceComboWidth() {
    if (!m_SourceCombo) {
        return;
    }

    const QFontMetrics metrics(m_SourceCombo->font());
    int widestProvider = 0;
    for (const auto& provider : m_Controller.providers()) {
        widestProvider = std::max(widestProvider, metrics.horizontalAdvance(provider.displayName));
    }

    const auto paneLimitedMax = std::clamp(width() / 3, minSourceComboMaxWidth, maxSourceComboWidth);
    const auto desiredWidth = std::clamp(widestProvider + sourceComboChromeWidth, minSourceComboWidth, paneLimitedMax);

    m_SourceCombo->setMinimumWidth(desiredWidth);
    m_SourceCombo->setMaximumWidth(desiredWidth);
}

void NifPreviewPane::updateTextureSourceComboItems() {
    m_UpdatingTextureControls = true;
    m_TextureSourceCombo->clear();
    for (const auto& provider : m_Controller.textureSources().providers) {
        m_TextureSourceCombo->addItem(provider.displayName);
        m_TextureSourceCombo->setItemData(m_TextureSourceCombo->count() - 1, provider.displayName, Qt::ToolTipRole);
    }
    m_TextureSourceCombo->setCurrentIndex(m_Controller.currentTextureSourceIndex());
    m_UpdatingTextureControls = false;

    updateTextureSourceComboWidth();
    updateTextureControls();
}

void NifPreviewPane::updateTextureControls() {
    const auto& textureSources = m_Controller.textureSources();
    const auto currentTextureSourceIndex = m_Controller.currentTextureSourceIndex();
    const auto hasMultipleTextureSources = textureSources.providers.size() > 2;

    m_PrevTextureButton->setEnabled(hasMultipleTextureSources);
    m_NextTextureButton->setEnabled(hasMultipleTextureSources);
    m_TextureSourceCombo->setEnabled(hasMultipleTextureSources);

    const auto toolTip = makeTextureToolTipText(textureSources, currentTextureSourceIndex);
    m_TextureLabel->setText(makeTextureSummaryText(textureSources));
    m_TextureLabel->setToolTip(toolTip);
    m_TextureSourceCombo->setToolTip(toolTip);

    m_UpdatingTextureControls = true;
    m_TextureSourceCombo->setCurrentIndex(currentTextureSourceIndex);
    m_UpdatingTextureControls = false;
}

void NifPreviewPane::updateTextureSourceComboWidth() {
    if (!m_TextureSourceCombo) {
        return;
    }

    const QFontMetrics metrics(m_TextureSourceCombo->font());
    int widestProvider = 0;
    for (const auto& provider : m_Controller.textureSources().providers) {
        widestProvider = std::max(widestProvider, metrics.horizontalAdvance(provider.displayName));
    }

    const auto paneLimitedMax = std::clamp(width() / 3, minSourceComboMaxWidth, maxSourceComboWidth);
    const auto desiredWidth = std::clamp(widestProvider + sourceComboChromeWidth, minSourceComboWidth, paneLimitedMax);

    m_TextureSourceCombo->setMinimumWidth(desiredWidth);
    m_TextureSourceCombo->setMaximumWidth(desiredWidth);
}

void NifPreviewPane::loadCurrentProvider() {
    const auto result = m_Controller.loadCurrentProvider();
    m_TitleLabel->setText(result.title);
    m_StatsLabel->setText(result.statsText);
    updateTextureSourceComboItems();

    switch (result.status) {
        case PreviewPaneLoadStatus::NoProvider:
            m_TitleLabel->clear();
            setViewWidget(new QLabel(tr("No previewable NIF version"), this));
            return;
        case PreviewPaneLoadStatus::Failed: setViewWidget(new QLabel(tr("Failed to load preview"), this)); return;
        case PreviewPaneLoadStatus::Loaded: reloadCurrentNifWidget(); return;
    }
}

void NifPreviewPane::reloadCurrentNifWidget() {
    const auto nifFile = m_Controller.currentNifFile();
    if (!nifFile) {
        return;
    }

    auto* const nifWidget = new NifWidget(
        nifFile,
        m_Organizer,
        m_Camera,
        m_Controller.currentTextureSourceProvider(),
        false,
        this
    );
    nifWidget->setShowCollision(m_ShowCollision);
    nifWidget->setMinimumSize(240, 240);
    m_NifWidget = nifWidget;
    setViewWidget(nifWidget);
}

void NifPreviewPane::setViewWidget(QWidget* widget) {
    if (m_ViewWidget) {
        m_ViewLayout->removeWidget(m_ViewWidget);
        m_ViewWidget->deleteLater();
    }

    m_ViewWidget = widget;
    m_NifWidget = qobject_cast<NifWidget*>(widget);

    if (auto* const label = qobject_cast<QLabel*>(widget)) {
        label->setAlignment(Qt::AlignCenter);
    }

    m_ViewLayout->addWidget(widget, 1);
}
