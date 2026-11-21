#pragma once

#include "Data/DataFileSource.h"

#include <QObject>
#include <QStringList>
#include <QVector>

#include <optional>

namespace MOBase {
class IOrganizer;
}

// Changes which copy of a file the game loads by hiding and restoring loose copies, and undoes the last change.
class CopyActions final : public QObject {
    Q_OBJECT

public:
    CopyActions(MOBase::IOrganizer& a_organizer, QObject* a_parent);

    // Takes the copies in game order, hidden ones after the rest. The label names the file in messages.
    void MakeWinner(const QVector<DataFileSource>& a_copies, qsizetype a_index, const QString& a_label);
    void Restore(const DataFileSource& a_hiddenCopy, const QString& a_label);
    void Undo();

signals:
    // MO2 is refreshing after a change. Requests are ignored until it ends.
    void Waiting(const QString& a_message);
    // MO2 finished refreshing after a change, so file lists must be resolved again.
    void Applied(const QString& a_message, bool a_canUndo);
    void Failed(const QString& a_message);

private:
    struct Change {
        // New paths of files the change hid, and of files it restored.
        QStringList hidden;
        QStringList restored;
        QString modName;
    };

    void Commit(const Change& a_change, const QString& a_message, bool a_undoable);

    MOBase::IOrganizer* _organizer;
    std::optional<Change> _lastChange;
    bool _waiting = false;
};
