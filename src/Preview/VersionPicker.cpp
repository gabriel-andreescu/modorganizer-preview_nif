#include "Preview/VersionPicker.h"
#include "Preview/CopyActionState.h"
#include "Preview/CopyText.h"
#include "Preview/FlowLayout.h"

#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

namespace {
constexpr int kSpacing = 6;

QString VersionLabel(
    const NifPreviewProvider& a_provider,
    const QVector<DataFileSource>& a_copies,
    const qsizetype a_copyIndex
) {
    if (a_copyIndex >= 0) {
        return CopyText::Label(a_copies, a_copyIndex);
    }
    if (a_provider.kind == NifPreviewProviderKind::kInMemory) {
        return a_provider.displayName;
    }
    // A loose file outside the game's data, such as one in a disabled mod.
    return QObject::tr("%1 (not loaded)").arg(a_provider.displayName);
}

QString VersionLocation(const NifPreviewProvider& a_provider) {
    switch (a_provider.kind) {
        case NifPreviewProviderKind::kLooseFile: return QDir::toNativeSeparators(a_provider.absolutePath);
        case NifPreviewProviderKind::kArchive:   return QDir::toNativeSeparators(a_provider.archivePath);
        case NifPreviewProviderKind::kInMemory:  return {};
    }
    return {};
}
} // namespace

VersionPicker::VersionPicker(QWidget* a_parent)
    : QWidget(a_parent) {
    auto* const label = new QLabel(tr("Mesh:"), this);
    _combo = new QComboBox(this);
    _combo->setToolTip(tr("The copy of this mesh shown in the preview, highest priority first"));
    _combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _combo->setMinimumContentsLength(12);
    label->setBuddy(_combo);
    _hideOthersButton = new QPushButton(tr("Hide Other Copies"), this);
    _unhideButton = new QPushButton(tr("Unhide"), this);
    _unhideButton->setToolTip(tr("Unhide this copy of the mesh"));
    // The preview sits in a dialog, where Enter would otherwise press one of these.
    _hideOthersButton->setAutoDefault(false);
    _unhideButton->setAutoDefault(false);

    auto* const layout = new FlowLayout(this, kSpacing, kSpacing);
    layout->addWidget(label);
    layout->addWidget(_combo);
    layout->addWidget(_hideOthersButton);
    layout->addWidget(_unhideButton);
    layout->SetStretchWidget(_combo);

    connect(_combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int a_index) {
        UpdateActions();
        emit VersionSelected(a_index);
    });
    connect(_hideOthersButton, &QPushButton::clicked, this, [this] { emit HideOthersClicked(_combo->currentIndex()); });
    connect(_unhideButton, &QPushButton::clicked, this, [this] { emit UnhideClicked(_combo->currentIndex()); });
}

void VersionPicker::SetVersions(const QVector<NifPreviewProvider>& a_providers, const int a_current) {
    _providers = a_providers;
    const auto copies = ProviderCopies(_providers);

    const QSignalBlocker blocker(_combo);
    _combo->clear();
    for (int i = 0; i < _providers.size(); ++i) {
        _combo->addItem(VersionLabel(_providers[i], copies, ProviderCopyIndex(_providers, i)));
        _combo->setItemData(i, VersionLocation(_providers[i]), Qt::ToolTipRole);
    }
    _combo->setCurrentIndex(a_current);
    UpdateActions();
}

void VersionPicker::UpdateActions() {
    const auto copies = ProviderCopies(_providers);
    const auto state = CopyActionStateFor(copies, ProviderCopyIndex(_providers, _combo->currentIndex()));

    // With a single copy there is nothing to hide, so the pane shows no actions for it.
    _hideOthersButton->setVisible(copies.size() > 1);
    _hideOthersButton->setEnabled(state.canHideOthers);
    _hideOthersButton->setToolTip(state.hideOthersToolTip);
    _unhideButton->setVisible(state.canUnhide);
}
