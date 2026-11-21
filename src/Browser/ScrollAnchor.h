#pragma once

#include <QObject>
#include <QPersistentModelIndex>

class QTreeView;

// Keeps a scrolled tree's top row where it is while rows arrive above it. A tree at its top stays at its top.
class ScrollAnchor final : public QObject {
    Q_OBJECT

public:
    // Takes a tree that already has its model.
    explicit ScrollAnchor(QTreeView& a_tree);

private:
    void Remember();
    void Restore();

    QTreeView* _tree;
    QPersistentModelIndex _topRow;
};
