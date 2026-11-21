#include "Preview/TextureList.h"
#include "Preview/CopyText.h"
#include "Preview/RowCombo.h"
#include "Textures/Source.h"

#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>

#include <algorithm>

namespace {
constexpr int kTextureColumn = 0;
constexpr int kSlotColumn = 1;
constexpr int kModColumn = 2;
// More textures than this scroll.
constexpr int kMaxShownRows = 5;

// Shortens a path from its start, so a narrow column keeps the file name.
class PathDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void initStyleOption(QStyleOptionViewItem* a_option, const QModelIndex& a_index) const override {
        QStyledItemDelegate::initStyleOption(a_option, a_index);
        a_option->textElideMode = Qt::ElideLeft;
    }
};

QString SlotNames(const TextureResult& a_result) {
    QStringList names;
    for (const auto& use : a_result.uses) {
        if (!names.contains(use.slotName)) {
            names.append(use.slotName);
        }
    }
    return names.join(QStringLiteral(", "));
}

QString DataPath(const TextureResult& a_result) {
    return QDir::toNativeSeparators(a_result.file.dataPath.isEmpty() ? a_result.path : a_result.file.dataPath);
}

QString CopyLabel(const FileLoad& a_file, const qsizetype a_index) {
    const auto label = CopyText::Label(a_file.copies, a_index);
    const auto failed = a_index == a_file.usedCopy && a_file.status == FileLoadStatus::kFailed;
    return failed ? QObject::tr("%1 (couldn't be read)").arg(label) : label;
}

QString NoCopyText(const FileLoad& a_file) {
    return a_file.copies.isEmpty() ? QObject::tr("Not found") : QObject::tr("Hidden in every mod");
}

QString SourceLacksText(const FileLoad& a_file, const QString& a_sourceName) {
    return a_file.chosenSourceLacksFile
               ? QObject::tr("%1 has no copy of this texture, so the preview shows the winning file").arg(a_sourceName)
               : QString();
}
} // namespace

TextureList::TextureList(QWidget* a_parent)
    : QTreeWidget(a_parent)
    , _rowHeight(RowCombo().sizeHint().height()) {
    setHeaderLabels({tr("Texture"), tr("Slot"), tr("Mod")});
    setRootIsDecorated(false);
    setAlternatingRowColors(true);
    setAllColumnsShowFocus(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setItemDelegateForColumn(kTextureColumn, new PathDelegate(this));
    setContextMenuPolicy(Qt::CustomContextMenu);
    header()->setSectionsMovable(false);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(kTextureColumn, QHeaderView::Stretch);
    header()->setSectionResizeMode(kSlotColumn, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(kModColumn, QHeaderView::Stretch);

    connect(this, &QTreeWidget::currentItemChanged, this, &TextureList::CurrentTextureChanged);
}

void TextureList::SetRows(const QVector<Row>& a_rows, const QString& a_sourceName) {
    const auto* const current = Current();
    const auto currentKey = current ? TextureDataPathKey(current->path) : QString();

    {
        const QSignalBlocker blocker(this);
        clear();
        _rows = a_rows;
        for (int i = 0; i < _rows.size(); ++i) {
            AddRow(i, a_sourceName);
            if (!currentKey.isEmpty() && TextureDataPathKey(_rows[i].result.path) == currentKey) {
                setCurrentItem(topLevelItem(i));
            }
        }
    }
    updateGeometry();
    emit CurrentTextureChanged();
}

QSize TextureList::sizeHint() const {
    const auto shownRows = static_cast<int>(std::min<qsizetype>(_rows.size(), kMaxShownRows));
    return {
        QTreeWidget::sizeHint().width(),
        header()->sizeHint().height() + (shownRows * _rowHeight) + (2 * frameWidth()),
    };
}

const TextureResult* TextureList::Current() const {
    const auto index = indexOfTopLevelItem(currentItem());
    return index >= 0 ? &_rows[index].result : nullptr;
}

void TextureList::AddRow(const int a_index, const QString& a_sourceName) {
    const auto& [result, differs] = _rows[a_index];
    const auto& file = result.file;
    const auto path = DataPath(result);

    auto* const item = new QTreeWidgetItem(this);
    item->setSizeHint(kTextureColumn, {0, _rowHeight});
    item->setText(kTextureColumn, path);
    item->setToolTip(kTextureColumn, differs ? tr("%1\nDiffers from the other pane").arg(path) : path);
    item->setText(kSlotColumn, SlotNames(result));
    if (file.status != FileLoadStatus::kLoaded) {
        item->setIcon(kTextureColumn, style()->standardIcon(QStyle::SP_MessageBoxWarning));
    }
    if (differs) {
        auto font = item->font(kTextureColumn);
        font.setBold(true);
        for (const auto column : {kTextureColumn, kSlotColumn, kModColumn}) {
            item->setFont(column, font);
        }
    }

    // A texture whose every copy is hidden shows none, and the dropdown still offers the hidden ones.
    if (file.copies.size() > 1 || (file.usedCopy < 0 && !file.copies.isEmpty())) {
        AddCopyCombo(item, result, a_sourceName);
        return;
    }

    if (file.usedCopy < 0) {
        item->setText(kModColumn, NoCopyText(file));
        return;
    }
    item->setText(kModColumn, CopyLabel(file, file.usedCopy));
    const auto lacks = SourceLacksText(file, a_sourceName);
    item->setToolTip(kModColumn, lacks.isEmpty() ? QDir::toNativeSeparators(file.copies[file.usedCopy].path) : lacks);
}

void TextureList::AddCopyCombo(QTreeWidgetItem* a_item, const TextureResult& a_result, const QString& a_sourceName) {
    const auto& file = a_result.file;

    auto* const combo = new RowCombo(this);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(8);
    combo->setPlaceholderText(NoCopyText(file));
    for (qsizetype i = 0; i < file.copies.size(); ++i) {
        combo->addItem(CopyLabel(file, i));
        combo->setItemData(static_cast<int>(i), QDir::toNativeSeparators(file.copies[i].path), Qt::ToolTipRole);
    }
    combo->setCurrentIndex(static_cast<int>(file.usedCopy));

    auto toolTip = tr("The copy of %1 shown in the preview, highest priority first")
                       .arg(QFileInfo(a_result.path).fileName());
    if (const auto lacks = SourceLacksText(file, a_sourceName); !lacks.isEmpty()) {
        toolTip = tr("%1\n%2").arg(toolTip, lacks);
    }
    combo->setToolTip(toolTip);

    connect(
        combo,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [this, a_item, key = TextureDataPathKey(a_result.path), copies = file.copies](const int a_index) {
            setCurrentItem(a_item);
            emit CopyChosen(key, copies[a_index].path);
        }
    );
    setItemWidget(a_item, kModColumn, combo);
}
