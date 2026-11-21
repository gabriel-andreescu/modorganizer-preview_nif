#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

class NoticeBar final : public QFrame {
    Q_OBJECT

public:
    explicit NoticeBar(QWidget* a_parent = nullptr);

    void Show(const QString& a_message, bool a_canUndo);

signals:
    void UndoClicked();

private:
    QLabel* _message = nullptr;
    QPushButton* _undoButton = nullptr;
    QPushButton* _dismissButton = nullptr;
};
