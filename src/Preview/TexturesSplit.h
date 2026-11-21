#pragma once

#include <QSplitter>

// The viewport above the textures panel. The panel gets the height its content needs and the viewport the rest, until
// the handle is dragged.
class TexturesSplit final : public QSplitter {
    Q_OBJECT

public:
    TexturesSplit(QWidget* a_viewport, QWidget* a_panel, QWidget* a_parent = nullptr);

    void FitPanel();

protected:
    void resizeEvent(QResizeEvent* a_event) override;

private:
    QWidget* _panel = nullptr;
    bool _dragged = false;
};
