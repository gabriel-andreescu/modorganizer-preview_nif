#include "Preview/TexturesPanel.h"
#include "Preview/CopyActionState.h"
#include "Preview/CopyText.h"
#include "Preview/FlowLayout.h"
#include "Preview/TextureList.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
QString UsedCopyPath(const TextureResult& a_result) {
    const auto& file = a_result.file;
    return file.usedCopy >= 0 ? file.copies[file.usedCopy].path : QString();
}

QString DifferText(const qsizetype a_count) {
    return a_count == 1 ? QObject::tr("1 differs") : QObject::tr("%1 differ").arg(a_count);
}

constexpr int kSpacing = 6;
} // namespace

TexturesPanel::TexturesPanel(QWidget* a_parent)
    : QWidget(a_parent) {
    auto* const sourceLabel = new QLabel(tr("Textures:"), this);
    _sourceCombo = new QComboBox(this);
    _sourceCombo->setToolTip(tr(
        "Where the textures come from. A mod supplies the textures it has, and the rest come from the winning files."
    ));
    _sourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _sourceCombo->setMinimumContentsLength(12);
    sourceLabel->setBuddy(_sourceCombo);

    _list = new TextureList(this);
    _hideOthersButton = new QPushButton(tr("Hide Other Copies"), this);
    _unhideButton = new QPushButton(tr("Unhide"), this);
    _unhideButton->setToolTip(tr("Unhide the shown copy of the selected texture"));
    _openButton = new QPushButton(tr("Open in Explorer"), this);
    _openButton->setToolTip(tr("Show the selected texture's file or archive in Explorer"));
    _differLabel = new QLabel(this);
    _differLabel->setToolTip(tr("Textures that differ from the other pane, shown in bold"));

    auto* const sourceRow = new QHBoxLayout();
    sourceRow->addWidget(sourceLabel);
    sourceRow->addWidget(_sourceCombo, 1);

    auto* const actionRow = new FlowLayout(nullptr, kSpacing, kSpacing);
    for (auto* const button : {_hideOthersButton, _unhideButton, _openButton}) {
        // The preview sits in a dialog, where Enter would otherwise press one of these.
        button->setAutoDefault(false);
        actionRow->addWidget(button);
    }
    actionRow->addWidget(_differLabel);

    auto* const layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(sourceRow);
    layout->addWidget(_list, 1);
    layout->addLayout(actionRow);

    connect(_sourceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int a_index) {
        // Custom stands for the current picks, so choosing it again changes nothing.
        const auto offset = _hasPicks ? 1 : 0;
        if (a_index >= offset) {
            emit SourceSelected(a_index - offset);
        }
    });
    connect(_list, &TextureList::CopyChosen, this, &TexturesPanel::PreviewCopy);
    connect(_list, &TextureList::CurrentTextureChanged, this, &TexturesPanel::UpdateActions);
    connect(_list, &QWidget::customContextMenuRequested, this, &TexturesPanel::ShowContextMenu);
    connect(_hideOthersButton, &QPushButton::clicked, this, &TexturesPanel::HideOthersOfCurrent);
    connect(_unhideButton, &QPushButton::clicked, this, &TexturesPanel::UnhideCurrent);
    connect(_openButton, &QPushButton::clicked, this, &TexturesPanel::OpenCurrentInExplorer);

    Rebuild();
}

void TexturesPanel::SetSources(const TextureSourceSet& a_sources, const int a_current, const bool a_hasPicks) {
    _hasPicks = a_hasPicks;
    _source = a_current >= 0 && a_current < a_sources.providers.size() ? a_sources.providers[a_current]
                                                                       : TextureSourceProvider {};

    const QSignalBlocker blocker(_sourceCombo);
    _sourceCombo->clear();
    if (a_hasPicks) {
        _sourceCombo->addItem(tr("Custom"));
    }
    for (const auto& provider : a_sources.providers) {
        _sourceCombo->addItem(provider.displayName);
    }
    _sourceCombo->setCurrentIndex(a_hasPicks ? 0 : a_current);
}

void TexturesPanel::SetTextures(const QVector<TextureResult>& a_results) {
    _results = a_results;
    Rebuild();
}

void TexturesPanel::SetComparison(std::optional<QVector<TextureResult>> a_other) {
    if (!a_other && !_other) {
        return;
    }

    _other = std::move(a_other);
    Rebuild();
}

void TexturesPanel::Rebuild() {
    QVector<TextureList::Row> rows;
    for (const auto& result : _results) {
        rows.push_back({.result = result, .differs = Differs(result)});
    }
    const auto gameIsSource = _source.kind == TextureSourceProviderKind::kGameData;
    _list->SetRows(rows, gameIsSource ? CopyText::Owner({}) : _source.sourceName);

    const auto differing = std::ranges::count_if(rows, &TextureList::Row::differs);
    _differLabel->setText(DifferText(differing));
    _differLabel->setVisible(differing > 0);
}

void TexturesPanel::UpdateActions() {
    const auto* const current = _list->Current();
    const auto state = current ? CopyActionStateFor(current->file.copies, current->file.usedCopy) : CopyActionState {};

    _hideOthersButton->setEnabled(state.canHideOthers);
    if (current == nullptr) {
        _hideOthersButton->setToolTip(tr("Select a texture first"));
    } else if (current->file.usedCopy < 0) {
        _hideOthersButton->setToolTip(tr("The preview shows no copy of this texture"));
    } else {
        _hideOthersButton->setToolTip(state.hideOthersToolTip);
    }
    _unhideButton->setEnabled(state.canUnhide);
    _openButton->setEnabled(current != nullptr && current->file.usedCopy >= 0);
}

void TexturesPanel::ShowContextMenu(const QPoint& a_position) {
    if (_list->itemAt(a_position) == nullptr) {
        return;
    }

    QMenu menu(this);
    menu.setToolTipsVisible(true);
    auto* const hideOthers = menu.addAction(_hideOthersButton->text(), this, &TexturesPanel::HideOthersOfCurrent);
    hideOthers->setEnabled(_hideOthersButton->isEnabled());
    hideOthers->setToolTip(_hideOthersButton->toolTip());
    menu.addAction(_unhideButton->text(), this, &TexturesPanel::UnhideCurrent)->setEnabled(_unhideButton->isEnabled());
    menu.addSeparator();
    menu.addAction(_openButton->text(), this, &TexturesPanel::OpenCurrentInExplorer)
        ->setEnabled(_openButton->isEnabled());
    menu.addAction(tr("Copy Path"), this, &TexturesPanel::CopyCurrentPath);
    menu.exec(_list->viewport()->mapToGlobal(a_position));
}

void TexturesPanel::HideOthersOfCurrent() {
    if (const auto* const current = _list->Current()) {
        emit HideOthers(*current, current->file.usedCopy);
    }
}

void TexturesPanel::UnhideCurrent() {
    if (const auto* const current = _list->Current()) {
        emit Unhide(*current, current->file.usedCopy);
    }
}

void TexturesPanel::OpenCurrentInExplorer() const {
    if (const auto* const current = _list->Current()) {
        QProcess::startDetached(
            QStringLiteral("explorer.exe"),
            {QStringLiteral("/select,"), QDir::toNativeSeparators(UsedCopyPath(*current))}
        );
    }
}

void TexturesPanel::CopyCurrentPath() const {
    if (const auto* const current = _list->Current()) {
        const auto& file = current->file;
        QApplication::clipboard()->setText(file.dataPath.isEmpty() ? current->path : file.dataPath);
    }
}

bool TexturesPanel::Differs(const TextureResult& a_result) const {
    if (!_other) {
        return false;
    }

    const auto key = TextureDataPathKey(a_result.path);
    const auto other = std::ranges::find_if(*_other, [&](const TextureResult& a_candidate) {
        return TextureDataPathKey(a_candidate.path) == key;
    });
    return other != _other->end() && UsedCopyPath(*other) != UsedCopyPath(a_result);
}
