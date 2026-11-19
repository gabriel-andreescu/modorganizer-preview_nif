#include "Preview/CopyActions.h"
#include "Data/FileHiding.h"
#include "Data/ModDataRefresh.h"
#include "Data/WinnerPlan.h"

namespace {
QStringList Paths(const QVector<DataFileSource>& a_copies) {
    QStringList paths;
    for (const auto& copy : a_copies) {
        paths.append(copy.path);
    }
    return paths;
}

QString HidMessage(const qsizetype a_count, const QString& a_label) {
    return a_count == 1 ? QObject::tr("Hid 1 other copy of %1.").arg(a_label)
                        : QObject::tr("Hid %1 other copies of %2.").arg(a_count).arg(a_label);
}

QString UnhidAndHidMessage(const qsizetype a_count, const QString& a_label) {
    return a_count == 1 ? QObject::tr("%1 is no longer hidden. Hid 1 other copy.").arg(a_label)
                        : QObject::tr("%1 is no longer hidden. Hid %2 other copies.").arg(a_label).arg(a_count);
}
} // namespace

CopyActions::CopyActions(MOBase::IOrganizer& a_organizer, QObject* a_parent)
    : QObject(a_parent)
    , _organizer(&a_organizer) {}

void CopyActions::MakeWinner(const QVector<DataFileSource>& a_copies, const qsizetype a_index, const QString& a_label) {
    if (_waiting) {
        return;
    }

    const auto plan = PlanWinner(a_copies, a_index);
    if (!plan.blocker.isEmpty()) {
        emit Failed(plan.blocker);
        return;
    }

    const auto restored = FileHiding::Restore(Paths(plan.toRestore));
    if (!restored.error.isEmpty()) {
        emit Failed(tr("Couldn't unhide the copy: %1").arg(restored.error));
        return;
    }

    const auto hidden = FileHiding::Hide(Paths(plan.toHide));
    if (!hidden.error.isEmpty()) {
        if (const auto rehidden = FileHiding::Hide(restored.renamed); !rehidden.error.isEmpty()) {
            qWarning("Failed to hide the restored copy again: %s", qUtf8Printable(rehidden.error));
        }
        emit Failed(tr("Couldn't hide the other copies: %1").arg(hidden.error));
        return;
    }

    const auto message = restored.renamed.isEmpty() ? HidMessage(hidden.renamed.size(), a_label)
                                                    : UnhidAndHidMessage(hidden.renamed.size(), a_label);
    Commit(
        {.hidden = hidden.renamed, .restored = restored.renamed, .modName = a_copies[a_index].modName},
        message,
        true
    );
}

void CopyActions::Restore(const DataFileSource& a_hiddenCopy, const QString& a_label) {
    if (_waiting) {
        return;
    }

    const auto restored = FileHiding::Restore({a_hiddenCopy.path});
    if (!restored.error.isEmpty()) {
        emit Failed(tr("Couldn't unhide the copy: %1").arg(restored.error));
        return;
    }
    Commit(
        {.hidden = {}, .restored = restored.renamed, .modName = a_hiddenCopy.modName},
        tr("%1 is no longer hidden.").arg(a_label),
        true
    );
}

void CopyActions::Undo() {
    if (_waiting || !_lastChange) {
        return;
    }

    const auto change = *_lastChange;
    const auto restored = FileHiding::Restore(change.hidden);
    if (!restored.error.isEmpty()) {
        emit Failed(tr("Couldn't undo: %1").arg(restored.error));
        return;
    }

    const auto hidden = FileHiding::Hide(change.restored);
    if (!hidden.error.isEmpty()) {
        if (const auto rehidden = FileHiding::Hide(restored.renamed); !rehidden.error.isEmpty()) {
            qWarning("Failed to hide the copies again: %s", qUtf8Printable(rehidden.error));
        }
        emit Failed(tr("Couldn't undo: %1").arg(hidden.error));
        return;
    }
    Commit({.hidden = hidden.renamed, .restored = restored.renamed, .modName = change.modName}, tr("Undone."), false);
}

void CopyActions::Commit(const Change& a_change, const QString& a_message, const bool a_undoable) {
    _lastChange = a_undoable ? std::optional(a_change) : std::nullopt;
    _waiting = true;
    emit Waiting(tr("Waiting for Mod Organizer to refresh..."));
    RefreshModData(*_organizer, a_change.modName, this, [this, a_message, a_undoable] {
        _waiting = false;
        emit Applied(a_message, a_undoable);
    });
}
