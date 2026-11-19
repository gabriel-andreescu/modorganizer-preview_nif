#pragma once

#include <QAbstractButton>
#include <QImage>

// An icon button drawn for the viewport: a white icon that brightens under the pointer. It paints itself, so MO2's
// themes, which style buttons for panels, leave it readable over the scene.
class ViewButton final : public QAbstractButton {
    Q_OBJECT

public:
    // Takes the path of an SVG icon.
    ViewButton(QString a_iconPath, const QString& a_toolTip, QWidget* a_parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* a_event) override;

private:
    [[nodiscard]] const QImage& Icon();

    QString _iconPath;
    // Rendered for the screen's pixel ratio, which changes when the window moves to another screen.
    QImage _icon;
    qreal _iconPixelRatio = 0.0;
};
