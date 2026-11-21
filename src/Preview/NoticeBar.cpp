#include "Preview/NoticeBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

NoticeBar::NoticeBar(QWidget* a_parent)
    : QFrame(a_parent) {
    setObjectName("nifPreviewNotice");

    _message = new QLabel(this);
    _message->setWordWrap(true);
    _message->setTextInteractionFlags(Qt::TextSelectableByMouse);

    _undoButton = new QPushButton(tr("Undo"), this);
    _dismissButton = new QPushButton(tr("Dismiss"), this);
    // The preview sits in a dialog, where Enter would otherwise press one of these.
    _undoButton->setAutoDefault(false);
    _dismissButton->setAutoDefault(false);

    auto* const layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_message, 1);
    layout->addWidget(_undoButton);
    layout->addWidget(_dismissButton);

    connect(_undoButton, &QPushButton::clicked, this, &NoticeBar::UndoClicked);
    connect(_dismissButton, &QPushButton::clicked, this, &QWidget::hide);
    hide();
}

void NoticeBar::Show(const QString& a_message, const bool a_canUndo) {
    _message->setText(a_message);
    _undoButton->setVisible(a_canUndo);
    show();
}
