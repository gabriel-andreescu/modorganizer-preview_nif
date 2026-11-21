#pragma once

#include <QComboBox>

// A combo box in a row of a scrolling list. The wheel scrolls the list and leaves the combo's choice alone.
class RowCombo final : public QComboBox {
    Q_OBJECT

public:
    using QComboBox::QComboBox;

protected:
    void wheelEvent(QWheelEvent* a_event) override;
};
