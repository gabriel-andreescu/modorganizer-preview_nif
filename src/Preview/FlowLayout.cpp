#include "Preview/FlowLayout.h"

#include <QWidget>

#include <algorithm>

FlowLayout::FlowLayout(QWidget* a_parent, const int a_horizontalSpacing, const int a_verticalSpacing)
    : QLayout(a_parent)
    , _horizontalSpacing(a_horizontalSpacing)
    , _verticalSpacing(a_verticalSpacing) {
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout() {
    qDeleteAll(_items);
}

void FlowLayout::SetStretchWidget(QWidget* a_widget) {
    _stretchWidget = a_widget;
    invalidate();
}

void FlowLayout::addItem(QLayoutItem* a_item) {
    _items.append(a_item);
}

int FlowLayout::count() const {
    return static_cast<int>(_items.size());
}

QLayoutItem* FlowLayout::itemAt(const int a_index) const {
    return _items.value(a_index);
}

QLayoutItem* FlowLayout::takeAt(const int a_index) {
    return a_index >= 0 && a_index < _items.size() ? _items.takeAt(a_index) : nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const {
    return {};
}

bool FlowLayout::hasHeightForWidth() const {
    return true;
}

int FlowLayout::heightForWidth(const int a_width) const {
    auto height = 0;
    for (const auto& row : RowsFor(a_width)) {
        height += row.height + (height > 0 ? _verticalSpacing : 0);
    }
    return height;
}

void FlowLayout::setGeometry(const QRect& a_rect) {
    QLayout::setGeometry(a_rect);

    auto top = a_rect.y();
    for (const auto& row : RowsFor(a_rect.width())) {
        PlaceRow(row, {a_rect.x(), top, a_rect.width(), row.height});
        top += row.height + _verticalSpacing;
    }
}

QSize FlowLayout::sizeHint() const {
    QSize size;
    for (const auto* const item : _items) {
        if (!item->isEmpty()) {
            const auto hint = item->sizeHint();
            size.rwidth() += hint.width() + (size.width() > 0 ? _horizontalSpacing : 0);
            size.setHeight(std::max(size.height(), hint.height()));
        }
    }
    return size;
}

QSize FlowLayout::minimumSize() const {
    QSize size;
    for (const auto* const item : _items) {
        if (!item->isEmpty()) {
            size = size.expandedTo(item->sizeHint());
        }
    }
    return size;
}

QList<FlowLayout::Row> FlowLayout::RowsFor(const int a_width) const {
    QList<Row> rows;
    for (auto* const item : _items) {
        if (item->isEmpty()) {
            continue;
        }

        const auto size = item->sizeHint();
        const auto startsRow = rows.isEmpty() || rows.last().width + _horizontalSpacing + size.width() > a_width;
        if (startsRow) {
            rows.append(Row {});
        }
        auto& row = rows.last();
        row.width += size.width() + (row.items.isEmpty() ? 0 : _horizontalSpacing);
        row.height = std::max(row.height, size.height());
        row.items.append(item);
    }
    return rows;
}

// Places the row's items side by side, each centered on the row's height. The stretch widget also takes the width
// the row leaves over.
void FlowLayout::PlaceRow(const Row& a_row, const QRect& a_rowRect) const {
    const auto spareWidth = std::max(0, a_rowRect.width() - a_row.width);
    auto left = a_rowRect.x();
    for (auto* const item : a_row.items) {
        auto size = item->sizeHint();
        if (_stretchWidget != nullptr && item->widget() == _stretchWidget) {
            size.rwidth() += spareWidth;
        }
        item->setGeometry({QPoint(left, a_rowRect.y() + ((a_rowRect.height() - size.height()) / 2)), size});
        left += size.width() + _horizontalSpacing;
    }
}
