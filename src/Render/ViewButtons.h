#pragma once

#include "Render/AxisView.h"

#include <QWidget>

// The buttons in the viewport's corner that turn the camera to the front, side and top views.
class ViewButtons final : public QWidget {
    Q_OBJECT

public:
    explicit ViewButtons(QWidget* a_parent = nullptr);

signals:
    void AxisClicked(ViewAxis a_axis);

protected:
    void paintEvent(QPaintEvent* a_event) override;

private:
    void AddButton(ViewAxis a_axis, const QString& a_iconFile, const QString& a_toolTip);
};
