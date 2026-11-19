#include "Render/ViewButtons.h"
#include "Render/ViewButton.h"

#include <QHBoxLayout>
#include <QPainter>

#include <uibase/imoinfo.h>

namespace {
constexpr int kPadding = 2;
constexpr qreal kCornerRadius = 6.0;
// Dark enough that the white icons stay readable over a bright mesh.
constexpr QColor kBacking {0, 0, 0, 110};
} // namespace

ViewButtons::ViewButtons(QWidget* a_parent)
    : QWidget(a_parent) {
    auto* const layout = new QHBoxLayout(this);
    layout->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    layout->setSpacing(0);

    AddButton(ViewAxis::kFront, QStringLiteral("view-front.svg"), tr("Front view. Click again for the back."));
    AddButton(ViewAxis::kSide, QStringLiteral("view-side.svg"), tr("Side view. Click again for the other side."));
    AddButton(ViewAxis::kTop, QStringLiteral("view-top.svg"), tr("Top view. Click again for the bottom."));
    adjustSize();
}

void ViewButtons::AddButton(const ViewAxis a_axis, const QString& a_iconFile, const QString& a_toolTip) {
    const auto iconPath = QStringLiteral("%1/icons/%2").arg(MOBase::IOrganizer::getPluginDataPath(), a_iconFile);
    auto* const button = new ViewButton(iconPath, a_toolTip, this);
    connect(button, &ViewButton::clicked, this, [this, a_axis] { emit AxisClicked(a_axis); });
    layout()->addWidget(button);
}

void ViewButtons::paintEvent(QPaintEvent* a_event) {
    Q_UNUSED(a_event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(kBacking);
    painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
}
