#pragma once

#include <QLayout>
#include <QList>

// Lays its items out in a row and wraps them onto further rows when the row is too narrow for all of them.
class FlowLayout final : public QLayout {
public:
    FlowLayout(QWidget* a_parent, int a_horizontalSpacing, int a_verticalSpacing);
    ~FlowLayout() override;
    FlowLayout(const FlowLayout&) = delete;
    FlowLayout(FlowLayout&&) = delete;
    FlowLayout& operator=(const FlowLayout&) = delete;
    FlowLayout& operator=(FlowLayout&&) = delete;

    // The widget that also takes the width its row leaves over. None by default.
    void SetStretchWidget(QWidget* a_widget);

    void addItem(QLayoutItem* a_item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int a_index) const override;
    QLayoutItem* takeAt(int a_index) override;
    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int a_width) const override;
    void setGeometry(const QRect& a_rect) override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSize() const override;

private:
    struct Row {
        QList<QLayoutItem*> items;
        int width = 0;
        int height = 0;
    };

    // The visible items split into the rows they take at this width.
    [[nodiscard]] QList<Row> RowsFor(int a_width) const;
    void PlaceRow(const Row& a_row, const QRect& a_rowRect) const;

    QList<QLayoutItem*> _items;
    QWidget* _stretchWidget = nullptr;
    int _horizontalSpacing;
    int _verticalSpacing;
};
