#include "Preview/TexturesSplit.h"

TexturesSplit::TexturesSplit(QWidget* a_viewport, QWidget* a_panel, QWidget* a_parent)
    : QSplitter(Qt::Vertical, a_parent)
    , _panel(a_panel) {
    addWidget(a_viewport);
    addWidget(a_panel);
    setStretchFactor(0, 1);
    setStretchFactor(1, 0);
    setChildrenCollapsible(false);

    // Only a drag emits this. Setting the sizes does not.
    connect(this, &QSplitter::splitterMoved, this, [this] { _dragged = true; });
}

void TexturesSplit::FitPanel() {
    if (_dragged || _panel->isHidden()) {
        return;
    }

    // The panel's size hint is for its narrowest width, where its buttons wrap onto more rows than they take here.
    const auto wanted = _panel->hasHeightForWidth() ? _panel->heightForWidth(width()) : _panel->sizeHint().height();
    setSizes({height() - handleWidth() - wanted, wanted});
}

void TexturesSplit::resizeEvent(QResizeEvent* a_event) {
    QSplitter::resizeEvent(a_event);
    FitPanel();
}
