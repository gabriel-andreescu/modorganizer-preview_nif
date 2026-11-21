#include "Browser/ScrollAnchor.h"

#include <QScrollBar>
#include <QTreeView>

ScrollAnchor::ScrollAnchor(QTreeView& a_tree)
    : QObject(&a_tree)
    , _tree(&a_tree) {
    const auto* const model = _tree->model();
    connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &ScrollAnchor::Remember);
    connect(model, &QAbstractItemModel::layoutChanged, this, &ScrollAnchor::Restore);
}

void ScrollAnchor::Remember() {
    _topRow = _tree->verticalScrollBar()->value() > 0 ? _tree->indexAt({0, 0}) : QModelIndex();
}

void ScrollAnchor::Restore() {
    if (_topRow.isValid()) {
        _tree->scrollTo(_topRow, QAbstractItemView::PositionAtTop);
    }
}
