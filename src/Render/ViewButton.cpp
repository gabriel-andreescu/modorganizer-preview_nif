#include "Render/ViewButton.h"

#include <QImageReader>
#include <QPainter>

#include <utility>

namespace {
constexpr int kButtonSize = 28;
constexpr int kIconSize = 20;
constexpr qreal kCornerRadius = 4.0;
constexpr qreal kIdleOpacity = 0.7;
constexpr QColor kHoverFill {255, 255, 255, 40};
constexpr QColor kPressedFill {255, 255, 255, 80};

QImage RenderIcon(const QString& a_path, const qreal a_pixelRatio) {
    QImageReader reader(a_path);
    reader.setScaledSize(QSize(kIconSize, kIconSize) * a_pixelRatio);
    auto icon = reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (icon.isNull()) {
        qWarning("Couldn't load the icon %s: %s", qUtf8Printable(a_path), qUtf8Printable(reader.errorString()));
        return icon;
    }

    // The icon files are black shapes. Filling the shapes keeps their outline and makes them white.
    QPainter painter(&icon);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(icon.rect(), Qt::white);
    painter.end();
    icon.setDevicePixelRatio(a_pixelRatio);
    return icon;
}
} // namespace

ViewButton::ViewButton(QString a_iconPath, const QString& a_toolTip, QWidget* a_parent)
    : QAbstractButton(a_parent)
    , _iconPath(std::move(a_iconPath)) {
    setToolTip(a_toolTip);
    setAttribute(Qt::WA_Hover);
    // The viewport keeps the keyboard focus, so its shortcuts still work after a click here.
    setFocusPolicy(Qt::NoFocus);
}

QSize ViewButton::sizeHint() const {
    return {kButtonSize, kButtonSize};
}

void ViewButton::paintEvent(QPaintEvent* a_event) {
    Q_UNUSED(a_event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (isDown() || underMouse()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(isDown() ? kPressedFill : kHoverFill);
        painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
    }

    const auto& icon = Icon();
    painter.setOpacity(underMouse() ? 1.0 : kIdleOpacity);
    painter.drawImage(QPoint((width() - kIconSize) / 2, (height() - kIconSize) / 2), icon);
}

const QImage& ViewButton::Icon() {
    if (const auto pixelRatio = devicePixelRatioF(); pixelRatio != _iconPixelRatio) {
        _icon = RenderIcon(_iconPath, pixelRatio);
        _iconPixelRatio = pixelRatio;
    }
    return _icon;
}
